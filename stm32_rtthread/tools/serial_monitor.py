import serial
import sys
import colorama
import threading

# 初始化 colorama 以支持 Windows 终端颜色
colorama.init()

# --- 配置参数 ---
SERIAL_PORT = 'COM3'  # 自动检测或根据实际情况修改
BAUD_RATE = 115200
# ----------------

def read_from_serial(ser):
    """后台运行：读取串口并输出"""
    while ser.is_open:
        try:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                sys.stdout.write(data.decode('utf-8', errors='ignore'))
                sys.stdout.flush()
        except Exception as e:
            print(f"\n\033[31m读取错误: {e}\033[0m")
            break

def start_monitor():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print(f"\033[32m--- 已连接到 {SERIAL_PORT} @ {BAUD_RATE} ---\033[0m")
        print("\033[33m--- [Ctrl+C 退出] | [输入文字后回车发送] ---\033[0m\n")
        
        # 启动后台读取线程
        read_thread = threading.Thread(target=read_from_serial, args=(ser,), daemon=True)
        read_thread.start()

        # 主线程处理发送
        while True:
            user_input = input() # 阻塞等待输入
            if user_input:
                # 默认追加 \r\n (嵌入式常用的行尾)
                ser.write((user_input + "\r\n").encode('utf-8'))
                
    except serial.SerialException as e:
        print(f"\033[31m无法打开串口: {e}\033[0m")
    except KeyboardInterrupt:
        print("\n\033[33m已断开串口并退出。\033[0m")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    start_monitor()
