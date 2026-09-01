#!/bin/sh
# ChatServer 开发容器启动脚本：容器启动时自动拉起 MySQL / RabbitMQ / http_server
# 挂载关系: 宿主机 /home/qq/CppAIService -> 容器 /root/httpserver_vsersion2
# 用法: 作为容器 CMD 运行; 可用环境变量 HTTP_PORT 指定端口(默认 8116)

PORT="${HTTP_PORT:-8116}"
log() { echo "[start-chatserver] $*"; }

# ---------- 1. MySQL: 带清理 + 重试, 解决容器冷启动偶发的 socket 冲突 ----------
start_mysql() {
    # 清理残留 socket/pid, 避免"another process is using unix socket"冲突
    rm -f /var/run/mysqld/mysqld.sock* /var/run/mysqld/mysqlx.sock* /var/run/mysqld/*.pid 2>/dev/null
    if command -v timeout >/dev/null 2>&1; then
        timeout 30 service mysql start >/tmp/mysql_start.log 2>&1
    else
        service mysql start >/tmp/mysql_start.log 2>&1
    fi
}

mysql_ready() { mysql -uroot -p123456 -e 'SELECT 1' >/dev/null 2>&1; }

try=1
while [ $try -le 3 ]; do
    mysql_ready && break
    log "MySQL 未就绪, 第 ${try} 次启动..."
    start_mysql
    i=0
    while [ $i -lt 10 ]; do
        mysql_ready && break
        sleep 1
        i=$((i+1))
    done
    mysql_ready && break
    try=$((try+1))
    sleep 2
done

if mysql_ready; then
    log "MySQL 就绪"
else
    log "WARN: MySQL 启动失败, 详见 /tmp/mysql_start.log:"
    cat /tmp/mysql_start.log 2>/dev/null | tail -20
fi

# ---------- 2. RabbitMQ ----------
service rabbitmq-server start >/tmp/rabbitmq_start.log 2>&1 || true
log "RabbitMQ 已尝试启动"

# ---------- 3. http_server ----------
pkill -f "http_server" >/dev/null 2>&1 || true
sleep 1
cd /root/httpserver_vsersion2/build
if [ -x ./http_server ]; then
    nohup ./http_server -p "$PORT" > /tmp/http_server.log 2>&1 &
    SERVER_PID=$!
    sleep 3
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        log "http_server 已启动 (pid $SERVER_PID, 端口 $PORT)"
    else
        log "WARN: http_server 启动后即退出, 日志:"
        cat /tmp/http_server.log 2>/dev/null | tail -20
    fi
else
    log "WARN: build/http_server 不存在, 请先编译: cd /root/httpserver_vsersion2/build && cmake .. && make"
fi

# ---------- 4. 保持容器存活 ----------
tail -f /dev/null
