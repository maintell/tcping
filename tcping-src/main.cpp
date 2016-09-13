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
	cout << "tcping ���ĵ����汾" << endl;
	cout << endl;
	cout << "�÷�: " << argv[0] << " [-����] Ŀ���ַ [�˿�]" << endl << endl;
	cout << "�÷�(��ȫ): " << argv[0] << " [-t] [-d] [-i ���] [-n ����] [-w ms] [-b n] [-r ����] [-s] [-v] [-j] [-js ������С] [-4] [-6] [-c] [-g ����] [-S Դ��ַ] [--file] [--tee ��־�ļ���] [-h] [-u] [--post] [--head] [--proxy-port �˿�] [--proxy-server ���������] [--proxy-credentials ����������û���:����] [-f] Ŀ���ַ " << "[�˿�]" << endl << endl;
	cout << " -t     : ��������Ctrl-Cֹͣ" << endl;
	cout << " -n 5   : ����ֻ����5��" << endl;
	cout << " -i 5   : ����ÿ5��һ��" << endl;
	cout << " -w 0.5 : ����ÿ��Ӧ��ֻ�ȴ�0.5��" << endl;
	cout << " -d     : ÿ�а�������ʱ��" << endl;
	cout << " -b 1   : ������ (1 ��ͨ����, 2 �Ӷϵ�ͨ," << endl;
	cout << "                        3 ״̬�仯, 4 �ܽ�)" << endl;
	cout << " -r 5   : ����ÿ5�ξͽ���һ��DNS���½���" << endl;
	cout << " -s     : �ɹ����Զ��˳�"<<endl;                  //[Modification 14 Apr 2011 by Michael Bray, mbray@presidio.com]
	cout << " -v     : �汾��" << endl;
	cout << " -j     : ����, Ĭ��ʹ���ۼ�ƽ��"<< endl;
	cout << " -js 5  : ������������ʶ���ٸ�ֵ���ۼ�ƽ��" << endl;
	cout << " --tee  : ���������ļ�" << endl;
	cout << " -4     : ����ipv4" << endl;
	cout << " -6     : ����ipv6" << endl;
	cout << " -c     : ����״̬�ı���ʾ" << endl;
	cout << " --file : ʹ���ļ���Ϊ��������ַ�����룬һ��һ����ѭ��ʹ��" << endl;
	cout << "          ע��: --file ѡ�����ڲ�ͬ��Ŀ���������-j��-c������" << endl;
	cout << "          ֻ���ܸ�ʽ�硰baidu.com 4443������" << endl;
	cout << " -g 5   : ���磬�������ʧ��5�η���" << endl;
	cout << " -S _X_ : �ض���Դ��ַ_X_.  Դ��ַ������һ���󶨵����������ĺϷ���IP��ַ" << endl;
	cout << endl << "HTTPѡ��:" << endl;
	cout << " -h     : HTTPģʽ(ʹ��url����ȥ��http://����ΪIP��ַ)" << endl;
	cout << " -u     : ÿ��һ��Ŀ��URL" << endl;
	cout << " --post : ʹ��POST����ʹ��GET(���⻺��)" << endl;
	cout << " --head : ʹ��HEAD����ʹ��GET" << endl;
	cout << " --proxy-server : ��������� " << endl;
	cout << " --proxy-port   : ����������˿� " << endl;
	cout << " --proxy-credentials : �ԡ��û���:���롱��ʽ����һ��'Proxy-Authorization: Basic'ͷ" << endl;
	cout << endl << "����ѡ��:" << endl;
	cout << " -f     : ǿ�����ٷ���һ���ֽ�" << endl;
	cout << " --header : ��httpͷ����ԭʼ�Ĳ�����ʱ������.  ʹ�ò���--tee��Ч." << endl;
	cout << " --block  : ʹ��blocking���͵�socket����������. ������ֹ��ֹ����-wʹ��Ĭ�ϵĳ�ʱʱ��(��ʱ����20��֮��)" << endl;
	cout << "            ����ͨ����ʱ�ķ�ʽ�����һЩ�ܾ����ӵĶ˿�." << endl;
	cout << endl << "\t�������ָ��Ŀ��˿�, Ĭ�϶˿�Ϊ��" << kDefaultServerPort << "." << endl;

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
			cout << endl << "** ��������.  ��Ctrl+C�˳� **" << endl;
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
			cout << endl << "** ������״̬�ı��ʱ����ʾ **" << endl;
		}

		// optional beepage
		if (!strcmp (argv[x], "-b")) {
			beep_mode = atoi(argv[x+1]);
			offset = x+1;
			switch (beep_mode) {
			case 0:
				break;
			case 1:
				cout << endl << "** ��ͨ���Ϸ������� **" << endl;
				break;
			case 2:
				cout << endl << "** �Ӷϵ�ͨ����һ�� **" << endl;
				break;
			case 3:
				cout << endl << "** ״̬�ı������һ�±�ʾ�Ӷϵ�ͨ�����±�ʾ��ͨ���� **" << endl;
				break;
			case 4:
				cout << endl << "** ����������һ�±�ʾ���������±�ʾ���� **" << endl;
				break;
			}

		}

		// dump version and quit
		if (!strcmp(argv[x], "-v") || !strcmp(argv[x], "--version") ) {
			//cout << "tcping.exe 0.30 Nov 13 2015" << endl;
			cout << "tcping.exe " << TCPING_VERSION << " " << TCPING_DATE << endl;
			cout << "����ʱ��: " << __DATE__ << " " << __TIME__ <<  endl;
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
		out.pf("ʱ��: %s %s\n", dateStr, timeStr);

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
			cout << "������Ŀ���ַǰ�����һ������, �ǲ��ǽ�������û�в���?" << endl;
			return 1;
	}

	int nPort = kDefaultServerPort;
	if (argc >= 3 + offset) {
		nPort = atoi(argv[2 + offset]);
	}

	// Do a little sanity checking because we're anal.
	int nNumArgsIgnored = (argc - 3 - offset);
	if (nNumArgsIgnored > 0) {
		cout << nNumArgsIgnored << " ����Ĳ���" << (nNumArgsIgnored == 1 ? "" : "s") << " ������." << endl;
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

		out.pf("\n** �� %s ���� %s:\n", server ,document);
		out.p("(��Ϊ����ԭ��, �ٶ�(kbit/s)�Ǹ�����ֵ )\n");
	}

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// Start Winsock up
	WSAData wsaData;
	int nCode;
	if ((nCode = WSAStartup(MAKEWORD(1, 1), &wsaData)) != 0) {
		cout << "WSAStartup() ���ش�����:" << nCode << "." << endl;
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

