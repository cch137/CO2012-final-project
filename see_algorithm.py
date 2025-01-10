import math
import matplotlib.pyplot as plt
import numpy as np

save_path = "./images/algorithm/"  # 替換為您的目標路徑

# 常數定義
selu_to_zero = 0.01
selu_to_less = 0.1

sigmoid_l = 1
sigmoid_k = 8  # 調整後的 sigmoid_k
sigmoid_mid = 0.5

relu_cut = 0.01

curve_begin = 1.0  # 調整為1以達到最大值

SELU_ALPHA = 1.67326324235
SELU_LAMBDA = 0.957  # 調整後的 lambda


# SELU 函數
def s_selu(input_val):
    if input_val < selu_to_zero:
        return 0.0
    elif input_val < selu_to_less:
        return (
            SELU_LAMBDA
            * SELU_ALPHA
            * (
                math.exp(input_val - selu_to_less)
                - math.exp(selu_to_zero - selu_to_less)
            )
        )
    else:
        return SELU_LAMBDA * input_val + (
            SELU_LAMBDA * SELU_ALPHA * (1 - math.exp(selu_to_zero - selu_to_less))
            - SELU_LAMBDA * selu_to_less
        )


# Sigmoid 函數
def s_sigmoid(input_val):
    return sigmoid_l / (1 + math.exp(-sigmoid_k * (input_val - sigmoid_mid)))


# Sigmoid 導數函數
def s_d_sigmoid(input_val):
    f = s_sigmoid(input_val)
    derivative = sigmoid_k * sigmoid_l * f * (1 - f / sigmoid_l)
    # 最大值歸一化
    max_derivative = sigmoid_k * sigmoid_l / 4
    normalized = derivative / max_derivative
    return normalized**2


# ReLU 函數
def s_create(x):
    if x < 0.3:
        return (-2) * x + 1
    elif x < 0.7:
        return s_create(0.7)
    else:
        return 2 * x - 1

    # 下方這段其實永遠不會被執行
    # （在 C 原碼裡也是如此）
    # return x if x > RELU_CUT else 0


# 平方函數
def s_square(input_val):
    return input_val * input_val


# 直接傳遞函數
def s_direct(input_val):
    return input_val


# S 曲線函數
def s_curve(input_val):
    return curve_begin * (1 - input_val) * (1 - input_val)


def s_075_025(x):
    return 0.75 * math.sqrt(x) + 0.25


def s_cube(x):
    return 4 * (x - 0.5) ** 3 + 0.5


# 生成輸入數據
inputs = np.linspace(0, 1, 500)

# 計算各函數的輸出
outputs_s_selu = [s_selu(x) for x in inputs]
outputs_s_sigmoid = [s_sigmoid(x) for x in inputs]
outputs_s_d_sigmoid = [s_d_sigmoid(x) for x in inputs]
outputs_s_create = [s_create(x) for x in inputs]
outputs_s_square = [s_square(x) for x in inputs]
outputs_s_direct = [s_direct(x) for x in inputs]
outputs_s_curve = [s_curve(x) for x in inputs]
outputs_s_075_025 = [s_075_025(x) for x in inputs]
outputs_s_cube = [s_cube(x) for x in inputs]

# 定義每個子圖的標題和對應的數據
functions = [
    ("s_selu", outputs_s_selu, "red"),
    ("s_sigmoid", outputs_s_sigmoid, "blue"),
    ("s_d_sigmoid", outputs_s_d_sigmoid, "green"),
    ("s_create", outputs_s_create, "purple"),
    ("s_square", outputs_s_square, "orange"),
    ("s_direct", outputs_s_direct, "brown"),
    ("s_curve", outputs_s_curve, "cyan"),
    ("s_075_025", outputs_s_075_025, "lime"),
    ("s_cube", outputs_s_cube, "magenta"),
]

# 繪製並保存每個函數的圖形
for name, output, color in functions:
    plt.figure(figsize=(7, 5))
    plt.plot(inputs, output, color=color, label=name)
    plt.title(f"{name} function")
    plt.xlabel("Input")
    plt.ylabel("Output")
    plt.ylim(-0.1, 1.2)  # 設置y軸範圍以便更好地觀察
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    # 保存為 JPG 文件
    plt.savefig(f"{save_path}{name}.jpg")
    plt.close()

print("所有圖形已保存為 JPG 文件！")
