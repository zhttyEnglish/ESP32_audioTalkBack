# 本项目为 ESP32s3板子使用声网sdk实现语音对讲

## 目录
paas-esp32s3-example/component/ 目录下为程序依赖的库

                    /main/      目录下为程序代码

## 版本
paas-esp32s3-example   为最新版本 包含 语音对讲 MQTT file_server 响铃 蓝牙配网等功能 ota功能受限于flash容量不够 不能使用

paas-esp32s3-example_back   为最初版本 只能进行语音通话 

## 环境搭建 

1. 解压 esp-adf.zip 
   
2. https://dl.espressif.cn/dl/idf-installer/esp-idf-tools-setup-offline-5.2.2.exe
在上面链接出下在 windows idf installer, 版本为 5.2.2 

3. 安装 windows idf installer 后， 会新建 esp idf power shell，打开power shell
   在窗口中执行 esp-adf 文件夹下的 install.bat 和 export.bat 

4. 命令执行后，可进入项目目录 在main下 执行 idf.py build 编译 
5. 更多 idf.py 命令 可参考 https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html
