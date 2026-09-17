#!/bin/sh
# ChatServer 开发容器启动脚本：容器启动时自动拉起 MySQL / RabbitMQ / http_server
# 用法: 作为容器 CMD 运行; 可用环境变量 HTTP_PORT 指定端口(默认 8116)
#
# 注意: 代码是否与宿主机同步取决于启动容器时是否绑定了挂载，例如
#   docker run -v /home/qq/CppAIService:/root/httpserver_vsersion2 ...
# 当前名为 ai-httpserver-2 的容器【没有】任何挂载，/root/httpserver_vsersion2
# 是镜像内的独立副本，宿主机改动不会自动进去，需要手动同步后重新编译。

PORT="${HTTP_PORT:-8116}"
log() { echo "[start-chatserver] $*"; }

# ---------- 0. AI 密钥 ----------
# 密钥集中放在 /root/.ai-keys（chmod 600，不进 git），这里加载，
# 使自动启动也能拿到最新的 Key / 模型名。
# 没有这个文件时，退回使用容器 docker run -e 传入的环境变量。
if [ -f /root/.ai-keys ]; then
    . /root/.ai-keys
    log "已加载 /root/.ai-keys"
fi

# ---------- 1. MySQL: 带清理 + 重试, 解决容器冷启动偶发的 socket 冲突 ----------
start_mysql() {
    # 只在 mysqld 确实没在运行时才清理残留 socket/pid。
    # 注意：如果 mysqld 正在运行，删掉它的 socket 文件会让它虽然活着却无法接受连接
    # （socket 的监听 fd 还在，但路径已 unlink，新连接找不到它），且无法自行恢复。
    if ! pgrep -x mysqld >/dev/null 2>&1; then
        rm -f /var/run/mysqld/mysqld.sock* /var/run/mysqld/mysqlx.sock* /var/run/mysqld/*.pid 2>/dev/null
    fi
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
