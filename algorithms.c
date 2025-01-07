#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "db/list.h"
#include "db/hash.h"

#include "database.h"
#include "social_network.h"
#include "algorithms.h"

#define SELU_TO_ZERO 0.01
#define SELU_TO_LESS 0.1

// 因為希望輸出的最大值接近1最小值接近0，所以調整k，原本應該是1
#define SIGMOID_L 1
#define SIGMOID_K 8
#define SIGMOID_MID 0.5

// 小於RELU_CUT的權重直接變0
#define RELU_CUT 0.01

// 1.33/1 = 0.25
#define CURVE_BEGIN 0.5

double default_recommand_algo(double x)
{
  return pow(x, 2.0);
}

double default_aggregate_algo(double x)
{
  return 0.75 * pow(x, 0.5) + 0.25;
}

// 將大於SELU_TO_LESS的值放大alpha倍
// 小於SELU_TO_LESS的值呈指數下降
// 將小於SELU_TO_ZERO的值變為0
double s_selu(double input)
{
  // 常數定義
  const double alpha = 1.67326324235;
  const double lambda = 0.957; // 原本應該要是1.05多的但我們希望他最大輸出值是1所以更改

  if (input < SELU_TO_ZERO)
  {
    // 區間一：輸出為0
    return 0.0;
  }
  else if (input < SELU_TO_LESS)
  {
    // 區間二：指數增長，確保在 SELU_TO_ZERO 處連續
    return lambda * alpha * (exp(input - SELU_TO_LESS) - exp(SELU_TO_ZERO - SELU_TO_LESS));
  }
  else
  {
    // 區間三：線性增長，確保在 SELU_TO_LESS 處連續
    return lambda * input + (lambda * alpha * (1 - exp(SELU_TO_ZERO - SELU_TO_LESS)) - lambda * SELU_TO_LESS);
  }
}

// 標準羅吉斯回歸函數，把x0變化
double s_sigmoid(double input)
{
  return SIGMOID_L / (1 + exp((-1) * SIGMOID_K * (input - SIGMOID_MID)));
}

// 回傳將input乘上在該點的斜率
double s_d_sigmoid(double input)
{
  double f = s_sigmoid(input);
  double derivative = SIGMOID_K * SIGMOID_L * f * (1 - f / SIGMOID_L);
  // 平方歸一化，讓最大值為1並且最小值接近0
  double max_derivative = SIGMOID_K * SIGMOID_L / 4;
  double normalized = derivative / max_derivative;
  return normalized * normalized;
}

// 小於RELU_CUT的就為0
double s_relu(double input)
{
  return input > RELU_CUT ? input : 0;
}

// 回傳input平方，希望讓大的放大，小的縮小
double s_square(double input)
{
  return input * input;
}

double s_direct(double input)
{
  return input;
}

double s_curve(double input)
{
  return CURVE_BEGIN * (input - 1) * (input - 1);
}
