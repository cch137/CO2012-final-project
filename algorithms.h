#ifndef ALGORITHMS_H
#define ALGORITHMS_H

// https://zh.wikipedia.org/zh-tw/%E6%BF%80%E6%B4%BB%E5%87%BD%E6%95%B0
// https://docs.google.com/document/d/1IQn-drdI7MEJm8X1O7h2gmDl1pjV78Kul_Yg95b6mL0/edit?tab=t.0

double r_algo0(double x);
double r_algo1(double x);
double r_algo2(double x);
double a_algo0(double x);
double a_algo1(double x);

// selu與relu的混和，分段，將大的線性變大，小的指數變小
double s_selu(double input);

// 標準羅吉斯回歸函數，把x0變化
double s_sigmoid(double input);

// 回傳將input乘上在該點的斜率
// 將原本的d_digmoid輸出平方歸一化，讓最大值為1並且最小值接近0
double s_d_sigmoid(double input);

// 回傳input平方，希望讓大的放大，小的縮小
double s_square(double input);

// 通過二次函數讓更改的比例逐漸下降，y=0.25(x-1)^2
double s_curve(double input);

// 小於RELU_CUT的就為0
double s_relu(double input);

// 不做任何權重
double s_direct(double input);

#endif