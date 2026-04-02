import serial
import struct
import time
import matplotlib
matplotlib.use('TkAgg')  # 强制使用交互式后端
import matplotlib.pyplot as plt
import numpy as np
import mplcursors
from scipy.fft import fft, fftfreq
from scipy.signal import butter, filtfilt

# --- 配置参数 ---
SERIAL_PORT = 'COM3'  # 请根据实际情况修改
BAUD_RATE = 115200
RECORD_TIME = 10      # 采集时长（秒）
ENABLE_FILTER = True  # 是否开启软件滤波
CUTOFF_FREQ = 10.0     # 截止频率 (Hz)，只保留 5Hz 以下的信号
FFT_POINTS = 8192     # FFT 补零点数，建议设为 2 的幂次 (如 1024, 2048)
# ----------------

def lowpass_filter(data, cutoff, fs, order=5):
    nyq = 0.5 * fs
    normal_cutoff = cutoff / nyq
    b, a = butter(order, normal_cutoff, btype='low', analog=False)
    y = filtfilt(b, a, data)
    return y

def collect_data():
    data_list = []
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        ser.reset_input_buffer()
        print(f"开始采集数据...")
        start_time = time.time()
        
        buffer = b""
        while (time.time() - start_time) < RECORD_TIME:
            # 批量读取缓冲区所有数据
            if ser.in_waiting > 0:
                buffer += ser.read(ser.in_waiting)
            
            # 处理缓冲区中的完整帧
            while len(buffer) >= 8: # 3字节头 + 4字节数据 + 1字节校验
                idx = buffer.find(b'\xAA\xBB\xCC')
                if idx == -1:
                    buffer = buffer[-2:] # 没找到头，保留最后两个字节防止头被切断
                    break
                if idx + 8 <= len(buffer):
                    frame = buffer[idx:idx+8]
                    data_bytes = frame[3:7]
                    checksum = frame[7]
                    if (sum(data_bytes) & 0xFF) == checksum:
                        val = struct.unpack('<f', data_bytes)[0]
                        data_list.append(val)
                        buffer = buffer[idx+8:] # 移除已处理帧
                    else:
                        buffer = buffer[idx+1:] # 校验失败，跳过当前头
                else:
                    break # 数据不足一帧，等待下次读取
        
        ser.close()
        print(f"采集完成，共获取 {len(data_list)} 个点。")
        return np.array(data_list)
    
    except Exception as e:
        print(f"错误: {e}")
        return None

def analyze_data(data):
    if len(data) < 10:
        print("采集到的有效数据点太少。")
        return

    # 1. 计算实际采样频率
    fs = len(data) / RECORD_TIME
    print(f"估算实际采样频率: {fs:.2f} Hz")
    
    # 打印前几个数据点进行调试
    print("前5个数据点:", data[:5])

    # 时间轴
    t = np.arange(len(data)) / fs

    # --- 滤波处理 ---
    if ENABLE_FILTER:
        # 截止频率不能超过采样频率的一半 (奈奎斯特频率)
        actual_cutoff = min(CUTOFF_FREQ, fs / 2 - 0.1)
        data_filtered = lowpass_filter(data, actual_cutoff, fs)
        plot_data = data_filtered
        label_suffix = f" (Filtered < {actual_cutoff}Hz)"
    else:
        plot_data = data
        label_suffix = " (Raw)"

    # 2. FFT 计算
    n = len(data)
    n_fft = FFT_POINTS  # 使用宏定义的补零点数
    window = np.hanning(n)
    xf = fftfreq(n_fft, 1/fs)[:n_fft//2]

    # 计算原始数据的 FFT
    data_raw_detrended = data - np.mean(data)
    yf_raw = fft(data_raw_detrended * window, n=n_fft)
    amplitude_raw = 2.0/n * np.abs(yf_raw[0:n_fft//2]) * 2.0

    # 如果开启了滤波，计算滤波后的 FFT
    if ENABLE_FILTER:
        data_filt_detrended = data_filtered - np.mean(data_filtered)
        yf_filt = fft(data_filt_detrended * window, n=n_fft)
        amplitude_filt = 2.0/n * np.abs(yf_filt[0:n_fft//2]) * 2.0
    
    # 3. 绘图
    plt.figure(figsize=(12, 8))

    # 时域波形
    plt.subplot(2, 1, 1)
    if ENABLE_FILTER:
        plt.plot(t, data, color='gray', alpha=0.5, label='Raw Data')
        plt.plot(t, data_filtered, color='blue', label='Filtered Data')
    else:
        plt.plot(t, data, color='blue', label='Raw Data')
    plt.title(f'Time Domain Waveform{label_suffix}')
    plt.xlabel('Time (s)')
    plt.ylabel('Acceleration (g)')
    plt.legend()
    plt.grid(True)

    # 频域波形
    plt.subplot(2, 1, 2)
    if ENABLE_FILTER:
        plt.plot(xf, amplitude_raw, color='gray', alpha=0.5, label='Raw FFT')
        plt.plot(xf, amplitude_filt, color='red', label='Filtered FFT')
        # 标注最大频率点（基于滤波后的数据）
        max_idx = np.argmax(amplitude_filt[1:]) + 1
        peak_amp = amplitude_filt[max_idx]
    else:
        plt.plot(xf, amplitude_raw, color='red', label='Raw FFT')
        max_idx = np.argmax(amplitude_raw[1:]) + 1
        peak_amp = amplitude_raw[max_idx]

    plt.title('Frequency Domain (FFT) Comparison')
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Amplitude')
    plt.legend()
    plt.grid(True)
    
    # 将频谱横轴限制到 0 - 50 Hz（如果采样率导致奈奎斯特频率 < 50Hz，则限制到实际最大频率）
    ax_freq = plt.gca()
    max_x = min(50.0, float(xf[-1]) if len(xf) > 0 else 50.0)
    ax_freq.set_xlim(0.0, max_x)

    # 标注最大频率点：如果峰值在可视范围内则直接标注，若峰值超出 50Hz 则在图右侧标注并说明真实频率
    if peak_amp > 0.001:
        peak_x = float(xf[max_idx])
        if peak_x <= max_x:
            ax_freq.annotate(f'Peak: {peak_x:.2f}Hz', xy=(peak_x, peak_amp), xytext=(peak_x + 1, peak_amp),
                             arrowprops=dict(facecolor='black', shrink=0.05))
        else:
            # 在视图右侧靠内的位置标注并显示真实频率
            ann_x = max_x * 0.95
            ax_freq.annotate(f'Peak: {peak_x:.2f}Hz (out of view)', xy=(ann_x, peak_amp), xytext=(ann_x - (max_x*0.1), peak_amp),
                             arrowprops=dict(facecolor='black', shrink=0.05))

    # 启用交互式光标
    cursor = mplcursors.cursor(hover=True)
    @cursor.connect("add")
    def _(sel):
        sel.annotation.set_text(f"X: {sel.target[0]:.2f}\nY: {sel.target[1]:.4f}")

    # --- MATLAB 风格的独立平移/缩放交互 ---
    # 实现要点：
    # - 左键按住并拖动在当前子图上实现平移（仅影响被拖动的子图）
    # - 鼠标滚轮在当前子图上实现以鼠标位置为中心的缩放（仅影响被操作的子图）
    class PanZoomInteractor:
        def __init__(self, fig, base_scale=2.0, pan_button=1):
            """
            改进说明：
            - 默认使用左键(pan_button=1)进行平移，和 MATLAB 更一致；
            - 使用像素->数据坐标变换计算平移，避免 data coords 在某些后端为 None 的问题；
            - 支持双击重置视图；在工具栏激活时不拦截事件以保留默认工具行为。
            """
            self.fig = fig
            self.base_scale = base_scale
            self.pan_button = pan_button
            self._press = None  # (ax, xpress_pix, ypress_pix, xlim, ylim, trans)
            # 存储每个轴的初始范围以便双击重置
            self._init_lims = {}
            fig.canvas.mpl_connect('button_press_event', self.on_press)
            fig.canvas.mpl_connect('button_release_event', self.on_release)
            fig.canvas.mpl_connect('motion_notify_event', self.on_motion)

        def _toolbar_is_active(self):
            try:
                toolbar = getattr(self.fig.canvas.manager, 'toolbar', None)
                if toolbar is None:
                    return False
                mode = getattr(toolbar, 'mode', '')
                return bool(mode)
            except Exception:
                return False

        def on_press(self, event):
            # 双击重置视图
            if event.dblclick and event.inaxes is not None:
                ax = event.inaxes
                if ax in self._init_lims:
                    ax.set_xlim(self._init_lims[ax][0])
                    ax.set_ylim(self._init_lims[ax][1])
                    ax.figure.canvas.draw_idle()
                return

            if event.inaxes is None or self._toolbar_is_active():
                return

            # 仅在指定按键按下时开始平移
            if event.button == self.pan_button:
                ax = event.inaxes
                # 保存初始范围（第一次按下时）
                if ax not in self._init_lims:
                    self._init_lims[ax] = (ax.get_xlim(), ax.get_ylim())
                # 使用像素坐标来避免 data coords 为 None 的问题
                self._press = (ax, event.x, event.y, ax.get_xlim(), ax.get_ylim(), ax.transData)

        def on_release(self, event):
            self._press = None

        def on_motion(self, event):
            if self._press is None:
                return
            ax, xpress, ypress, xlim0, ylim0, trans = self._press
            if event.inaxes != ax:
                return
            if event.x is None or event.y is None:
                return
            # 将像素位移转换为数据坐标位移：逆变换当前像素到数据
            inv = ax.transData.inverted()
            x0_data, y0_data = inv.transform((xpress, ypress))
            x1_data, y1_data = inv.transform((event.x, event.y))
            dx = x1_data - x0_data
            dy = y1_data - y0_data
            ax.set_xlim(xlim0[0] - dx, xlim0[1] - dx)
            ax.set_ylim(ylim0[0] - dy, ylim0[1] - dy)
            ax.figure.canvas.draw_idle()

        

    # 在当前图形上创建交互器（作用于所有子图，但每次只影响鼠标所在子图）
    # 使用左键拖拽进行平移（MATLAB 更常用的交互），缩放基数为 2
    inter = PanZoomInteractor(plt.gcf(), base_scale=2.0, pan_button=1)

    # 恢复并使用每个子图独立的滚轮缩放（接近最初实现，但只作用于 event.inaxes）
    def zoom_factory(ax, base_scale=2.0):
        def zoom_fun(event):
            if event.inaxes != ax:
                return
            # 如果工具栏处于活动模式，交给工具栏处理
            try:
                toolbar = getattr(ax.figure.canvas.manager, 'toolbar', None)
                if toolbar is not None and getattr(toolbar, 'mode', ''):
                    return
            except Exception:
                pass

            # 兼容不同后端：优先使用 event.step，其次使用 event.button
            direction = None
            step = getattr(event, 'step', None)
            if step is not None:
                direction = 'up' if step > 0 else 'down'
            else:
                btn = getattr(event, 'button', None)
                if btn in ('up', 'down'):
                    direction = btn
                elif isinstance(btn, int):
                    direction = 'up' if btn > 0 else 'down'

            if direction == 'up':
                scale_factor = 1.0 / base_scale
            elif direction == 'down':
                scale_factor = base_scale
            else:
                return

            xdata = event.xdata
            ydata = event.ydata
            if xdata is None or ydata is None:
                return
            cur_xlim = ax.get_xlim()
            cur_ylim = ax.get_ylim()
            new_width = (cur_xlim[1] - cur_xlim[0]) * scale_factor
            new_height = (cur_ylim[1] - cur_ylim[0]) * scale_factor

            relx = (xdata - cur_xlim[0]) / (cur_xlim[1] - cur_xlim[0]) if cur_xlim[1] != cur_xlim[0] else 0.5
            rely = (ydata - cur_ylim[0]) / (cur_ylim[1] - cur_ylim[0]) if cur_ylim[1] != cur_ylim[0] else 0.5

            ax.set_xlim([xdata - relx * new_width, xdata + (1 - relx) * new_width])
            ax.set_ylim([ydata - rely * new_height, ydata + (1 - rely) * new_height])
            ax.figure.canvas.draw_idle()

        fig = ax.get_figure()
        fig.canvas.mpl_connect('scroll_event', zoom_fun)

    # 为每个子图添加独立的滚轮缩放
    for ax in plt.gcf().axes:
        zoom_factory(ax, base_scale=2.0)

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    print("--- 程序已启动 ---")
    # 检查是否安装了必要库
    # pip install pyserial numpy matplotlib scipy
    captured_data = collect_data()
    if captured_data is not None:
        analyze_data(captured_data)
