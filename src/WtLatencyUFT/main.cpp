/*!
 * \file main.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT延迟测试工具的主程序入口
 * 
 * 本文件是WtLatencyUFT模块的主程序入口文件。
 * 该工具用于测试WonderTrader UFT（Ultra Fast Trading，超快交易）引擎的延迟性能。
 * 
 * 设计说明：
 * - 提供简单的命令行程序入口
 * - 调用test_uft()函数执行延迟测试
 * - 测试完成后等待用户按键退出
 * 
 * 功能：
 * - 初始化UFT引擎和测试环境
 * - 运行延迟测试
 * - 输出测试结果
 */

#include "../WTSTools/WTSLogger.h"  // WonderTrader日志工具

extern void test_uft();              // 外部函数声明：UFT延迟测试函数，在UftLatencyTool.cpp中定义

/**
 * @brief 主函数
 * @return 返回程序退出码（0表示成功）
 * 
 * 程序入口点：
 * 1. 调用test_uft()执行UFT延迟测试
 * 2. 输出提示信息
 * 3. 等待用户按键
 * 4. 返回退出码
 */
int main()
{
	test_uft();                      // 调用UFT延迟测试函数，执行测试并输出结果
	printf("press enter key to exit\r\n");  // 输出提示信息：按回车键退出
	getchar();                       // 等待用户输入回车键
	return 0;                        // 返回0表示程序正常退出
}

