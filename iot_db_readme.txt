
./start-cli.sh -h 192.168.2.35 -p 6667 -u root -pw root

-- 创建存储组
CREATE DATABASE root.sensor;

-- 创建时间序列
CREATE TIMESERIES root.sensor.dht11.temperature WITH DATATYPE=FLOAT, ENCODING=PLAIN;
CREATE TIMESERIES root.sensor.dht11.humidity WITH DATATYPE=FLOAT, ENCODING=PLAIN;

-- 验证创建结果
SHOW TIMESERIES root.sensor.dht11.*;