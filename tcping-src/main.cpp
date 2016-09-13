const char *TCPING_VERSION = "0.34";
const char *TCPING_DATE = "May 17 2016";

#pragma comment(lib, "Ws2_32.lib")

#include <winsock2.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>

#include "tee.h"
#include "tcping.h"


using namespace std;

void usage(int argc, char* argv[]) {
	cout << "--------------------------------------------------------------" << endl;
	cout << "tcping 中文导出版本" << endl;
	cout << endl;
	cout << "用法: " << argv[0] << " [-参数] 目标地址 [端口]" << endl << endl;
	cout << "用法(完全): " << argv[0] << " [-t] [-d] [-i 间隔] [-n 次数] [-w ms] [-b n] [-r 次数] [-s] [-v] [-j] [-js 抖动大小] [-4] [-6] [-c] [-g 次数] [-S 源地址] [--file] [--tee 日志文件名] [-h] [-u] [--post] [--head] [--proxy-port 端口] [--proxy-server 代理服务器] [--proxy-credentials 代理服务器用户名:密码] [-f] 目标地址 " << "[端口]" << endl << endl;
	cout << " -t     : 持续到按Ctrl-C停止" << endl;
	cout << " -n 5   : 比如只进行5次" << endl;
	cout << " -i 5   : 比如每5秒一次" << endl;
	cout << " -w 0.5 : 比如每次应答只等待0.5秒" << endl;
	cout << " -d     : 每行包含日期时间" << endl;
	cout << " -b 1   : 蜂鸣器 (1 从通到断, 2 从断到通," << endl;
	cout << "                        3 状态变化, 4 总叫)" << endl;
	cout << " -r 5   : 比如每5次就进行一次DNS重新解析" << endl;
	cout << " -s     : 成功后自动退出"<<endl;                  //[Modification 14 Apr 2011 by Michael Bray, mbray@presidio.com]
	cout << " -v     : 版本号" << endl;
	cout << " -j     : 抖动, 默认使用累计平均"<< endl;
	cout << " -js 5  : 抖动，参数标识多少个值的累计平均" << endl;
	cout << " --tee  : 输出结果到文件" << endl;
	cout << " -4     : 优先ipv4" << endl;
	cout << " -6     : 优先ipv6" << endl;
	cout << " -c     : 仅当状态改变显示" << endl;
	cout << " --file : 使用文件作为服务器地址的输入，一行一个，循环使用" << endl;
	cout << "          注意: --file 选项由于不同的目标服务器与-j和-c不兼容" << endl;
	cout << "          只接受格式如“baidu.com 4443”输入" << endl;
	cout << " -g 5   : 比如，如果连续失败5次放弃" << endl;
	cout << " -S _X_ : 特定的源地址_X_.  源地址必须是一个绑定到本地网卡的合法的IP地址" << endl;
	cout << endl << "HTTP选项:" << endl;
	cout << " -h     : HTTP模式(使用url但是去除http://来作为IP地址)" << endl;
	cout << " -u     : 每行一个目标URL" << endl;
	cout << " --post : 使用POST而不使用GET(避免缓存)" << endl;
	cout << " --head : 使用HEAD而不使用GET" << endl;
	cout << " --proxy-server : 代理服务器 " << endl;
	cout << " --proxy-port   : 代理服务器端口 " << endl;
	cout << " --proxy-credentials : 以“用户名:密码”格式定义一个'Proxy-Authorization: Basic'头" << endl;
	cout << endl << "调试选型:" << endl;
	cout << " -f     : 强制至少发送一个字节" << endl;
	cout << " --header : 在http头包含原始的参数和时间日期.  使用参数--tee有效." << endl;
	cout << " --block  : 使用blocking类型的socket来进行连接. 用来防止防止参数-w使用默认的超时时间(有时候有20秒之长)" << endl;
	cout << "            可以通过超时的方式来检测一些拒绝连接的端口." << endl;
	cout << endl << "\t如果您不指定目标端口, 默认端口为：" << kDefaultServerPort << "." << endl;

}


int main(int argc, char* argv[]) {

	// Do we have enough command line arguments?
	if (argc < 2) {
		usage(argc, argv);
		return 1;
	}

	int times_to_ping = 4;
	int offset = 0;  // because I don't feel like writing a whole command line parsing thing, I just want to accept an optional -t.  // well, that got out of hand quickly didn't it? -Future Eli
	double ping_interval = 1;
	int include_timestamp = 0;
	int beep_mode = 0;  // 0 is off, 1 is down, 2 is up, 3 is on change, 4 is constantly
	int ping_timeout = 2000;
	bool blocking = false;
	int relookup_interval = -1;
	int auto_exit_on_success = 0;
	int force_send_byte = 0;

	int include_url = 0;
	int use_http = 0;
	int http_cmd = 0;

	int include_jitter = 0;
	int jitter_sample_size = 0;

	int only_changes = 0;

	// for http mode
	char *serverptr;
	char *docptr = NULL;
	char server[2048];
	char document[2048];

	// for --tee
	char logfile[256];
	int use_logfile = 0;
	int show_arg_header = 0;

	// preferred IP version
	int ipv = 0;

	// http proxy server and port
	int proxy_port = 3128;
	char proxy_server[2048];
	proxy_server[0] = 0;

	char proxy_credentials[2048];
	int using_credentials = 0;

	// Flags for "read from filename" support
	int no_statistics = 0;  // no_statistics flag kills the statistics finale in the cases where we are reading entries from a file
	int reading_from_file = 0;  // setting this flag so we can mangle the other settings against it post parse.  For instance, it moves the meaning of -n and -t
	char urlfile[256];
	int file_times_to_loop = 1;
	bool file_loop_count_was_specific = false;   // ugh, since we are taking over the -n and -t options, but we don't want a default of 4 but we *do* want 4 if they specified 4

	int giveup_count = 0;

	int use_source_address = 0;
	char* src_address = "";

	for (int x = 0; x < argc; x++) {

		if (!strcmp(argv[x], "/?") || !strcmp(argv[x], "?") || !strcmp(argv[x], "--help") || !strcmp(argv[x], "-help")) {
			usage(argc, argv);
			return 1;
		}

		if (!strcmp(argv[x], "--proxy-port")) {
			proxy_port = atoi(argv[x + 1]);
			offset = x + 1;
		}

		if (!strcmp(argv[x], "--proxy-server")) {
			sprintf_s(proxy_server, sizeof(proxy_server), argv[x + 1]);
			offset = x + 1;
		}

		if (!strcmp(argv[x], "--proxy-credentials")) {
			sprintf_s(proxy_credentials, sizeof(proxy_credentials), argv[x + 1]);
			using_credentials = 1;
			offset = x + 1;
		}

		// force IPv4
		if (!strcmp(argv[x], "-4")) {
			ipv = 4;
			offset = x;
		}

		// force IPv6
		if (!strcmp(argv[x], "-6")) {
			ipv = 6;
			offset = x;
		}

		// ping continuously
		if (!strcmp(argv[x], "-t")) {
			times_to_ping = -1;
			file_loop_count_was_specific = true;
			offset = x;
			cout << endl << "** 持续工作.  按Ctrl+C退出 **" << endl;
		}

		// Number of times to ping
		if (!strcmp(argv[x], "-n")) {
			times_to_ping = atoi(argv[x + 1]);
			file_loop_count_was_specific = true;
			offset = x + 1;
		}

		// Give up
		if (!strcmp(argv[x], "-g")) {
			giveup_count = atoi(argv[x + 1]);
			offset = x + 1;
		}

		// exit on first successful ping
		if (!strcmp(argv[x], "-s")) {
			auto_exit_on_success = 1;
			offset = x;
		}

		if (!strcmp(argv[x], "--header")) {
			show_arg_header = 1;
			offset = x;
		}

		if (!strcmp(argv[x], "--block")) {
			blocking = true;
			offset = x;
		}

		// tee to a log file
		if (!strcmp(argv[x], "--tee")) {
			strcpy_s(logfile, sizeof(logfile), static_cast<const char*>(argv[x + 1]));
			offset = x + 1;
			use_logfile = 1;
			show_arg_header = 1;
		}

		// read from a text file
		if (!strcmp(argv[x], "--file")) {
			offset = x;
			no_statistics = 1;
			reading_from_file = 1;
		}

		// http mode
		if (!strcmp(argv[x], "-h")) {
			use_http = 1;
			offset = x;
		}

		// http mode - use get
		if (!strcmp(argv[x], "--get")) {
			use_http = 1; //implied
			http_cmd = HTTP_GET;
			offset = x;
		}

		// http mode - use head
		if (!strcmp(argv[x], "--head")) {
			use_http = 1; //implied
			http_cmd = HTTP_HEAD;
			offset = x;
		}

		// http mode - use post
		if (!strcmp(argv[x], "--post")) {
			use_http = 1; //implied
			http_cmd = HTTP_POST;
			offset = x;
		}

		// include url per line
		if (!strcmp(argv[x], "-u")) {
			include_url = 1;
			offset = x;
		}

		// force send a byte
		if (!strcmp(argv[x], "-f")) {
			force_send_byte = 1;
			offset = x;
		}

		// interval between pings
		if (!strcmp(argv[x], "-i")) {
			ping_interval = atof(argv[x+1]);
			offset = x+1;
		}

		// wait for response
		if (!strcmp(argv[x], "-w")) {
			ping_timeout = (int)(1000 * atof(argv[x + 1]));
			offset = x+1;
		}

		// source address
		if (!strcmp(argv[x], "-S")) {
			src_address = argv[x + 1];
			use_source_address = 1;
			offset = x + 1;
		}

		// optional datetimestamp output
		if (!strcmp(argv[x], "-d")) {
			include_timestamp = 1;
			offset = x;
		}

		// optional jitter output
		if (!strcmp(argv[x], "-j")) {
			include_jitter = 1;
			offset = x;
		}
	 
		// optional jitter output (sample size)
		if (!strcmp(argv[x], "-js")) {
			include_jitter = 1;
			offset = x;

			// obnoxious special casing if they actually specify the default 0
			if (!strcmp(argv[x+1], "0")) {
				jitter_sample_size = 0;
				offset = x+1;
			} else {
				if (atoi(argv[x+1]) == 0) {
					offset = x;
				} else {
					jitter_sample_size = atoi(argv[x+1]);
					offset = x+1;
				}
			}
			//			cout << "offset coming out "<< offset << endl;
		}

		// optional hostname re-lookup
		if (!strcmp(argv[x], "-r")) {
			relookup_interval = atoi(argv[x+1]);
			offset = x+1;
		}
		
		 // optional output minimization
		if (!strcmp(argv[x], "-c")) {
			only_changes = 1;
			offset = x;
			cout << endl << "** 仅仅在状态改变的时候显示 **" << endl;
		}

		// optional beepage
		if (!strcmp (argv[x], "-b")) {
			beep_mode = atoi(argv[x+1]);
			offset = x+1;
			switch (beep_mode) {
			case 0:
				break;
			case 1:
				cout << endl << "** 从通到断蜂鸣两下 **" << endl;
				break;
			case 2:
				cout << endl << "** 从断到通蜂鸣一下 **" << endl;
				break;
			case 3:
				cout << endl << "** 状态改变蜂鸣，一下表示从断到通，两下表示从通到断 **" << endl;
				break;
			case 4:
				cout << endl << "** 持续蜂鸣，一下表示正常，两下表示断了 **" << endl;
				break;
			}

		}

		// dump version and quit
		if (!strcmp(argv[x], "-v") || !strcmp(argv[x], "--version") ) {
			//cout << "tcping.exe 0.30 Nov 13 2015" << endl;
			cout << "tcping.exe " << TCPING_VERSION << " " << TCPING_DATE << endl;
			cout << "编译时间: " << __DATE__ << " " << __TIME__ <<  endl;
			cout << endl;
			return 1;
		}
	}

	// open our logfile, if applicable
	tee out;
	if (use_logfile == 1 && logfile != NULL) {
		out.Open(logfile);
	}



	if (show_arg_header == 1) {
		out.p("-----------------------------------------------------------------\n");
		// print out the args
		out.p("args: ");
		for (int x = 0; x < argc; x++) {
			out.pf("%s ", argv[x]);
		}
		out.p("\n");


		// and the date

		time_t rawtime;
		struct tm  timeinfo;
		char dateStr[11];
		char timeStr[9];

		errno_t err;

		_strtime_s(timeStr, sizeof(timeStr));

		time(&rawtime);

		err = localtime_s(&timeinfo, &rawtime);
		strftime(dateStr, 11, "%Y:%m:%d", &timeinfo);
		out.pf("时间: %s %s\n", dateStr, timeStr);

		// and the attrib
		out.pf("tcping.exe v%s:\n", TCPING_VERSION);
		out.p("-----------------------------------------------------------------\n");

	}





	// Get host and (optionally) port from the command line

	char* pcHost = "";
	//char pcHost[2048] = "";
	
	if (argc >= 2 + offset) {
		if (!reading_from_file) {
			pcHost = argv[1 + offset];
		}
		else {
			strcpy_s(urlfile, sizeof(urlfile), static_cast<const char*>(argv[offset + 1]));
		}


	} else {
			cout << "请检查在目标地址前的最后一个参数, 是不是仅有命令没有参数?" << endl;
			return 1;
	}

	int nPort = kDefaultServerPort;
	if (argc >= 3 + offset) {
		nPort = atoi(argv[2 + offset]);
	}

	// Do a little sanity checking because we're anal.
	int nNumArgsIgnored = (argc - 3 - offset);
	if (nNumArgsIgnored > 0) {
		cout << nNumArgsIgnored << " 额外的参数" << (nNumArgsIgnored == 1 ? "" : "s") << " 被忽略." << endl;
	}

	if (use_http == 1 && reading_from_file == 0) {   //added reading from file because if we are doing multiple http this message is just spam.
		serverptr = strchr(pcHost, ':');
		if (serverptr != NULL) {
			++serverptr;
			++serverptr;
			++serverptr;
		} else {
			serverptr = pcHost;
		}

		docptr = strchr(serverptr, '/');
		if (docptr != NULL) {
			*docptr = '\0';
			++docptr;

			strcpy_s(server, sizeof(server), static_cast<const char*>(serverptr));
			strcpy_s(document, sizeof(document), static_cast<const char*>(docptr));
		} else {
			strcpy_s(server, sizeof(server), static_cast<const char*>(serverptr));
			document[0] = '\0';
		}

		out.pf("\n** 从 %s 请求 %s:\n", server ,document);
		out.p("(因为各种原因, 速度(kbit/s)是个估计值 )\n");
	}

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// Start Winsock up
	WSAData wsaData;
	int nCode;
	if ((nCode = WSAStartup(MAKEWORD(1, 1), &wsaData)) != 0) {
		cout << "WSAStartup() 返回错误码:" << nCode << "." << endl;
		return 255;
	}

	// Call the main example routine.
	int retval;

	out.p("\n");

	if (!reading_from_file) {
		retval = DoWinsock_Single(pcHost, nPort, times_to_ping, ping_interval, include_timestamp, beep_mode, ping_timeout, relookup_interval, auto_exit_on_success, force_send_byte, include_url, use_http, docptr, http_cmd, include_jitter, jitter_sample_size, logfile, use_logfile, ipv, proxy_server, proxy_port, using_credentials, proxy_credentials, only_changes, no_statistics, giveup_count, out, use_source_address, src_address, blocking);
	}
	else {
		if (file_loop_count_was_specific) {
			file_times_to_loop = times_to_ping;
		}
		times_to_ping = 1;
		retval = DoWinsock_Multi(pcHost, nPort, times_to_ping, ping_interval, include_timestamp, beep_mode, ping_timeout, relookup_interval, auto_exit_on_success, force_send_byte, include_url, use_http, docptr, http_cmd, include_jitter, jitter_sample_size, logfile, use_logfile, ipv, proxy_server, proxy_port, using_credentials, proxy_credentials, only_changes, no_statistics, giveup_count, file_times_to_loop, urlfile, out, use_source_address, src_address, blocking);
	}

	// Shut Winsock back down and take off.
	WSACleanup();
	return retval;
}

