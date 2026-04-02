import serial
import numpy as np
import cv2
import struct
import time

# ================= 配置区 =================
SERIAL_PORT = 'COM6'  
BAUD_RATE = 4000000   

# 分辨率配置: QVGA=320x240, VGA=640x480
IMG_WIDTH = 320
IMG_HEIGHT = 240
# ==========================================

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        ser.reset_input_buffer()
        print(f"--- 开启串口 {SERIAL_PORT} (Baud: {BAUD_RATE}) ---")
    except Exception as e:
        print(f"无法打开串口: {e}")
        return

    # --- 匹配最新的 main.c 协议 ---
    HEAD = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x11, 0x22, 0x33])
    TAIL = bytes([0x33, 0x22, 0x11, 0x00, 0xDD, 0xCC, 0xBB, 0xAA])
    
    buffer = b''
    
    # FPS 和数据统计变量
    fps = 0.0
    frame_count = 0
    total_size = 0  # 累计数据大小
    avg_size_kb = 0.0
    start_time = time.time()
    
    while True:
        if ser.in_waiting > 0:
            buffer += ser.read(ser.in_waiting)
        
        # 找头
        idx = buffer.find(HEAD)
        if idx == -1:
            if len(buffer) > 1024 * 1024: buffer = b'' # 防溢出
            continue
            
        # 确保收够 头(8) + 长度(4)
        if len(buffer) < idx + 12:
            continue
            
        data_len = struct.unpack('<I', buffer[idx+8:idx+12])[0]
        
        # 确保收够 头(8) + 长度(4) + 数据 + 尾(8)
        total_len = 8 + 4 + data_len + 8
        if len(buffer) < idx + total_len:
            continue
            
        # 提取 Payload
        img_payload = buffer[idx+12 : idx+12+data_len]
        footer = buffer[idx+12+data_len : idx+total_len]
        
        # 验证尾部
        if footer == TAIL:
            try:
                # 解析 JPEG 数据
                nparr = np.frombuffer(img_payload, np.uint8)
                img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                
                if img is not None:
                    # 更新统计数据
                    frame_count += 1
                    total_size += data_len
                    elapsed = time.time() - start_time
                    
                    if elapsed >= 1.0:
                        fps = frame_count / elapsed
                        avg_size_kb = (total_size / frame_count) / 1024.0
                        print(f"Stats: FPS={fps:.1f}, AvgSize={avg_size_kb:.1f} KB, Bandwidth={(total_size*8/elapsed/1000000):.2f} Mbps")
                        
                        frame_count = 0
                        total_size = 0
                        start_time = time.time()
                    
                    # 绘制实时的显示信息
                    cv2.putText(img, f"FPS: {fps:.1f}", (10, 30), 
                                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                    cv2.putText(img, f"Size: {avg_size_kb:.1f} KB", (10, 60), 
                                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

                    cv2.imshow('ESP32-S3 Camera Stream', img)
                    if cv2.waitKey(1) & 0xFF == ord('q'): break
            except Exception as e:
                print(f"解析失败: {e}")
            
            buffer = buffer[idx + total_len:] # 移动指针
        else:
            # 如果尾部校验失败，尝试在当前 buffer 中找下一个头，而不是只跳过一个字节
            buffer = buffer[idx + 1:] 
            
    ser.close()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
