import matplotlib
# 使用 TkAgg 后端，这是 Python 自带的，不需要额外安装 Qt
matplotlib.use('TkAgg') 
import matplotlib.pyplot as plt
import threading
import serial
import struct
import time
import sys

# --- 配置参数 ---
SERIAL_PORT = 'COM3'   # 请根据实际情况修改串口号
BAUD_RATE = 115200
# ----------------

# 全局变量
is_recording = False
stop_thread = False
data_buffer = {
    'target_vx': [],
    'target_vy': [],
    'vx': [],
    'vy': [],
    'pos_x': [], # originally ax/tx
    'pos_y': [], # originally ay/ty
    'ang_x': [],
    'ang_y': [],
    't':  []
}

# 串口读取线程
def read_serial():
    global is_recording
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print(f"已连接串口 {SERIAL_PORT}")
    except Exception as e:
        print(f"串口打开失败: {e}")
        return

    # 帧格式：HEAD(0xAA 0xAA) + 8 * float
    # 1. target_vx (3-6)
    # 2. target_vy (7-10)
    # 3. vx        (11-14)
    # 4. vy        (15-18)
    # 5. pos_x     (19-22)
    # 6. pos_y     (23-26)
    # 7. ang_x     (27-30)
    # 8. ang_y     (31-34)
    # 总长 = 2 + 8 * 4 = 34 字节
    FRAME_LEN = 34
    buffer = b""

    print("串口监听中(二进制模式)...")
    
    while not stop_thread:
        try:
            if ser.in_waiting > 0:
                chunk = ser.read(ser.in_waiting)
                buffer += chunk

                while len(buffer) >= FRAME_LEN:
                    # 查找帧头 0xAA 0xAA
                    head_idx = buffer.find(b'\xAA\xAA')
                    if head_idx == -1:
                        # 没找到头，保留最后1个字节(防止AA被切断)，其余丢弃
                        buffer = buffer[-1:] 
                        break
                    
                    # 检查剩余长度是否足够一帧
                    if len(buffer) < head_idx + FRAME_LEN:
                        break # 数据不够，等待下次
                    
                    # 提取一帧数据 (跳过前2个字节头部)
                    frame_data = buffer[head_idx+2 : head_idx+FRAME_LEN]
                    
                    # 只有在记录模式下才解析存储
                    if is_recording:
                        try:
                            # 8个float
                            vals = struct.unpack('<ffffffff', frame_data)
                            # 目标速度取反，以便与实际速度同向对比(解决坐标系极性问题)
                            data_buffer['target_vx'].append(-vals[0])
                            data_buffer['target_vy'].append(-vals[1])
                            data_buffer['vx'].append(vals[2])
                            data_buffer['vy'].append(vals[3])
                            data_buffer['pos_x'].append(vals[4])
                            data_buffer['pos_y'].append(vals[5])
                            data_buffer['ang_x'].append(vals[6])
                            data_buffer['ang_y'].append(vals[7])
                            
                            data_buffer['t'].append(time.time())
                        except struct.error:
                            pass # 解析失败忽略

                    # 移除已处理的数据
                    buffer = buffer[head_idx + FRAME_LEN:]
            
            else:
                time.sleep(0.001)

        except Exception as e:
            print(f"读取错误: {e}")
            break
    ser.close()

def main():
    global is_recording, stop_thread
    
    # 启动串口线程
    t = threading.Thread(target=read_serial, daemon=True)
    t.start()

    print("\n--- 交互式采集工具 (二进制极速版) ---")

    while True: # 主循环：支持多次绘图
        # 清空可能存在的输入缓冲区 (限制次数防止死循环)
        try:
            import msvcrt
            cnt = 0
            while msvcrt.kbhit() and cnt < 10000:
                msvcrt.getch()
                cnt += 1
        except ImportError:
            pass

        print("\n==================================")
        print("1. 确保设备已连接并正在运行。")
        user_in = input("2. 按 [回车] 键开始记录数据 (输入 'q' 退出)...")
        if user_in.lower() == 'q':
            stop_thread = True
            break
        
        # 开始记录
        if 'data_buffer' in globals(): # 清空数据
            for key in data_buffer:
                data_buffer[key].clear()
        
        is_recording = True
        start_t = time.time()
        print(f"\n[正在记录] 数据实时收集中... (已开始于 {time.ctime()})")
        
        # 再次清空缓冲区，防止第一次回车的残留或者连击
        time.sleep(1) 
        try:
            import msvcrt
            cnt = 0
            while msvcrt.kbhit() and cnt < 10000:
                msvcrt.getch()
                cnt += 1
        except ImportError:
            pass

        input("3. 按 [回车] 键停止记录并生成图表...")
        
        # 停止记录
        is_recording = False
        # 注意：这里不再设置 stop_thread = True，以便后台线程继续运行
        
        duration = time.time() - start_t
        count = len(data_buffer['t'])
        print(f"\n[记录结束] 共耗时 {duration:.2f} 秒, 采集 {count} 个点。正在绘图...")

        if count == 0:
            print("警告：未采集到数据。请检查STM32是否发送了以此格式开头的数据: AA AA [float]...")
            continue # 继续下一次循环

        # --- 新增：静止误差方差分析 ---
        if count > 10:
            import numpy as np
            var_pos_x = np.var(data_buffer['pos_x'])
            var_pos_y = np.var(data_buffer['pos_y'])
            var_vel_x = np.var(data_buffer['vx'])
            var_vel_y = np.var(data_buffer['vy'])
            print(f"\n[静止误差方差分析] (假设当前为静止状态)")
            print(f"  位置 X 方差: {var_pos_x:.6f} | Y 方差: {var_pos_y:.6f}")
            print(f"  速度 X 方差: {var_vel_x:.6f} | Y 方差: {var_vel_y:.6f}")
            print(f"  可以作为 Kalman R 值参考 (位置方差 ≈ R)")
        # ---------------------

        # 生成图表
        fig = plt.figure(figsize=(12, 8))
        
        # --- 交互处理器：支持滚轮缩放 + 左键拖动 ---
        class InteractionHandler:
            def __init__(self, figure):
                self.fig = figure
                self.ax = None
                self.press = False
                self.start_xlim = None
                self.start_ylim = None
                self.start_ex = None
                self.start_ey = None
                self.vlines = []

                self.fig.canvas.mpl_connect('scroll_event', self.on_scroll)
                self.fig.canvas.mpl_connect('button_press_event', self.on_press)
                self.fig.canvas.mpl_connect('button_release_event', self.on_release)
                self.fig.canvas.mpl_connect('motion_notify_event', self.on_motion)

            def init_lines(self):
                # --- 新增：垂直辅助线 ---
                self.vlines = []
                for ax in self.fig.axes:
                    # 在每个子图中初始化一条不可见的垂直线
                    vl = ax.axvline(x=0, color='gray', linestyle='--', linewidth=0.8, alpha=0.8)
                    vl.set_visible(False)
                    self.vlines.append(vl)
                # ---------------------

            def on_scroll(self, event):
                if event.inaxes is None: return
                ax = event.inaxes
                
                # button='up' 是滚轮向上(放大), 'down' 是向下(缩小)
                scale = 0.8 if event.button == 'up' else 1.2
                
                xlim = ax.get_xlim()
                ylim = ax.get_ylim()
                
                xdata, ydata = event.xdata, event.ydata
                if xdata is None or ydata is None: return

                new_xlim = [
                    xdata - (xdata - xlim[0]) * scale,
                    xdata + (xlim[1] - xdata) * scale
                ]
                new_ylim = [
                    ydata - (ydata - ylim[0]) * scale,
                    ydata + (ylim[1] - ydata) * scale
                ]
                
                ax.set_xlim(new_xlim)
                ax.set_ylim(new_ylim)
                self.fig.canvas.draw_idle()

            def on_press(self, event):
                # 仅响应左键 (button 1) 且在坐标轴内
                if event.button != 1 or event.inaxes is None: return
                self.ax = event.inaxes
                self.press = True
                # 记录初始状态
                self.start_xlim = self.ax.get_xlim()
                self.start_ylim = self.ax.get_ylim()
                self.start_ex = event.x
                self.start_ey = event.y

            def on_release(self, event):
                self.press = False
                self.ax = None

            def on_motion(self, event):
                # --- 新增：更新垂直线位置 ---
                if event.inaxes:
                    # 鼠标在某个坐标轴内，只在当前子图显示垂直线
                    x_coord = event.xdata
                    current_ax = event.inaxes
                    
                    for vl in self.vlines:
                        if vl.axes == current_ax:
                            vl.set_xdata([x_coord, x_coord])
                            vl.set_visible(True)
                        else:
                            vl.set_visible(False) # 隐藏其他子图的线
                    self.fig.canvas.draw_idle()
                else:
                    # 鼠标移出坐标轴，隐藏所有垂直线
                    # 检查是否需要重绘（如果有一条是可见的，就隐藏并重绘）
                    needs_redraw = False
                    for vl in self.vlines:
                        if vl.get_visible():
                            vl.set_visible(False)
                            needs_redraw = True
                    
                    if needs_redraw:
                        self.fig.canvas.draw_idle()
                # ---------------------------

                if not self.press or self.ax is None: return
                
                # 计算鼠标移动的像素距离
                dx = event.x - self.start_ex
                dy = event.y - self.start_ey
                
                bbox = self.ax.get_window_extent()
                range_x = self.start_xlim[1] - self.start_xlim[0]
                range_y = self.start_ylim[1] - self.start_ylim[0]
                
                # 将像素差转换为数据差
                dx_data = dx * (range_x / bbox.width)
                dy_data = dy * (range_y / bbox.height)
                
                # 反向平移视口
                self.ax.set_xlim(self.start_xlim[0] - dx_data, self.start_xlim[1] - dx_data)
                self.ax.set_ylim(self.start_ylim[0] - dy_data, self.start_ylim[1] - dy_data)
                
                self.fig.canvas.draw_idle()

        # --- 交互处理器：支持滚轮缩放 + 左键拖动 ---
        handler = InteractionHandler(fig)
        # ---------------------------------------------

        # 时间轴归零
        base_time = data_buffer['t'][0]
        times = [t - base_time for t in data_buffer['t']]

        # 子图1：X轴数据
        ax1 = plt.subplot(2, 1, 1)
        plt.title(f"X-Axis: Target Vel vs Real Vel ({count} samples)")
        plt.plot(times, data_buffer['target_vx'], label='Target Vel X', color='magenta', linewidth=1.5, linestyle='--')
        plt.plot(times, data_buffer['vx'],        label='Real Vel X',   color='blue',    linewidth=1)
        plt.plot(times, data_buffer['pos_x'],     label='Position X',   color='green',   linewidth=1, alpha=0.6)
        plt.plot(times, data_buffer['ang_x'],     label='Servo Ang X',  color='red',     linewidth=1, alpha=0.6)
        plt.ylabel("Value")
        plt.legend(loc='upper right')
        plt.grid(True)
        
        # 子图2：Y轴数据
        ax2 = plt.subplot(2, 1, 2)
        plt.title("Y-Axis: Target Vel vs Real Vel")
        plt.plot(times, data_buffer['target_vy'], label='Target Vel Y', color='magenta', linewidth=1.5, linestyle='--')
        plt.plot(times, data_buffer['vy'],        label='Real Vel Y',   color='blue',    linewidth=1)
        plt.plot(times, data_buffer['pos_y'],     label='Position Y',   color='green',   linewidth=1, alpha=0.6)
        plt.plot(times, data_buffer['ang_y'],     label='Servo Ang Y',  color='red',     linewidth=1, alpha=0.6)
        plt.xlabel("Time (s)")
        plt.ylabel("Value")
        plt.legend(loc='upper right')
        plt.grid(True)

        # 延迟初始化 Handler (确保 axes 已经创建)
        handler.init_lines()

        plt.tight_layout()
        print("正在显示图表...请在查看完毕后关闭图表窗口以继续。")
        plt.show() # 此处会阻塞，直到窗口关闭
        
        # 显式清理资源
        plt.close('all')
        try:
            del handler
        except:
            pass
        
        print("\n图表已关闭。")
        # 窗口关闭后，继续下一次循环

if __name__ == '__main__':
    main()
