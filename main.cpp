#include <csignal>
#include <sys/stat.h>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/filesystem.hpp>
#include <Logger/Logger.h>
#include <Config/Config.h>
#include <Socket/TcpServer.h>
#include <Helpers/Helpers.h>
#include <Socket/FileServer.h>
#include <boost/thread.hpp>

#define _count_of(arg) ( (sizeof arg) / (sizeof arg[0]) )


/**
 * Remove before this
 */
Logger logger;
Config *_config;

std::string agent_pid_file = "/var/run/fpanel-agent.pid";
std::string debug_log_file = "/var/log/fpanel/agent-debug.logs";
std::string error_log_file = "/var/log/fpanel/agent-error.logs";
Helpers helpers;

static void control(int sig)
{
    static const char *labels[] = { "-", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE", "SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS", "SIGPIPE", "SIGALRM", "SIGTERM" };
    if (sig > 0 && sig < (int)_count_of(labels))
        fprintf(stderr, "Something bad %s happened.\n", labels[sig]);
    else
        fprintf(stderr, "Something bad %d happened.\n", sig);
    exit(sig);
}

static void errorHandler(int sig) {
    size_t nStackTraces = 20;
    void *array[nStackTraces];
    size_t size;
    size = backtrace(array, nStackTraces);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(0);
}

enum {
    SecondLog = 1,
    MaxLogFileSize = 104857600 // 100MB
};

void writePidToFile(pid_t pid)
{
    if(helpers.IsFileExists(agent_pid_file)) {
        helpers.RemoveFile(agent_pid_file);
    }
    std::ofstream pid_file (agent_pid_file);
    if (pid_file.is_open()) {
        pid_file << pid;
        pid_file.close();
    }
}

void initLogs()
{
    plog::init(plog::debug, debug_log_file.c_str(), MaxLogFileSize);              // Initialize the default logger instance.
    plog::init<SecondLog>(plog::error, error_log_file.c_str(), MaxLogFileSize);   // Initialize the 2nd logger instance.
}

static void daemonize()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    signal(SIGSEGV, errorHandler); // Error handling
//    signal(SIGCHLD, SIG_IGN);
//    signal(SIGHUP, SIG_IGN);

    //@TODO: Implement a working signal handler */
    signal(SIGCHLD, &control);
    signal(SIGABRT, &control);
    signal(SIGALRM, &control);
    signal(SIGILL, &control);
    signal(SIGFPE, &control);
    signal(SIGHUP, &control);
    signal(SIGINT, &control);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
}

boost::program_options::variables_map commandLine(int argc, char** argv)
{
    boost::program_options::variables_map vm;

    try {
        bool t_given = false;

        boost::program_options::options_description desc("Allowed options");
        desc.add_options()
                // First parameter describes option name/short name
                // The second is parameter to option
                // The third is description
                ("help,h", boost::program_options::bool_switch(&t_given), "print usage message")
                ("debug,d", boost::program_options::bool_switch(&t_given), "Open debug mode")
                ("systemd,D", boost::program_options::bool_switch(&t_given), "Run for systemd")
                ("quit,q", boost::program_options::bool_switch(&t_given), "Quit from program")
                ;

        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help") && (vm["help"].as<bool>())) {
            std::cout << "FPanel Agent Ver 1.0.0, for Linux (amd64)" << std::endl;
            std::cout << "Copyright (c) 2021 Kovengo Software Limited. All rights reserved." << std::endl;
            std::cout << "" << std::endl;
            std::cout << "FPanel is a registered trademark of Kovengo Software Limited. Other names may be trademarks of their respective owners." << std::endl;
            std::cout << "" << std::endl;
            std::cout << "Allowed options:\n"
                         "  --help|-h                Print usage message\n"
                         "  --systemd|-D             Run for systemd\n"
                         "  --quit|-q                Stop process\n" << std::endl;
            exit(0);
        }

    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    return vm;
}

void CheckOs()
{
    std::string os_name = helpers.CmdOutput("cat /etc/os-release | grep ^PRETTY_NAME | sed -e 's/PRETTY_NAME=//g' -e 's/\"//g'");
    bool can_run = false;
    std::vector<std::string> selected_os = {"debian","centos","ubuntu"};
    helpers.ToLower(os_name);
    for(int i; i < selected_os.size(); ++i) {
        if(os_name.find(selected_os[i]) != std::string::npos) {
            if(selected_os[i].compare("centos") == 0) {
                helpers.CmdOutput("systemctl stop firewalld");
            }
            can_run = true;
            break;
        }
    }
    if(!can_run) {
        logger.Error("OS Cant detected.");
        helpers.Println("OS Cant detected.");
        exit(EXIT_FAILURE);
    }
}

void Start(int argc, char** argv)
{
    initLogs();
    if(getuid() != 0) {
        logger.Error("Please run as root user.");
        helpers.Println("Please run as root user.");
        exit(EXIT_FAILURE);
    }
    boost::program_options::variables_map vm = commandLine(argc, argv);

    CheckOs();

    bool debug = false;
    if (vm.count("debug") && (vm["debug"].as<bool>())) {
        helpers.Println("FPanel Agent starting as debug mode.");
        logger.debug = true;
        debug = true;
    }


    if (vm.count("systemd") && (vm["systemd"].as<bool>())) {
        helpers.Println("FPanel Agent starting as SystemD mode.");
    }
    else {
        if(!debug) {
            daemonize();
        }
    }

    _config = new Config();
    _config->setLogger(logger);
    _config->init();

    Commands *_commands = new Commands();
    _commands->SetLogger(logger)
            .SetDebug(logger.debug)
            .SetConfig(_config);

    std::string f_sock_addr = _config->getValue("file.socket.server.address");
    std::string f_sock_port = _config->getValue("file.socket.server.port");

    std::string sock_addr = _config->getValue("socket.server.address");
    std::string sock_port = _config->getValue("socket.server.port");
    logger.Debug("FPanel Agent starting as file transfer socket %s:%s", f_sock_addr.c_str(), f_sock_port.c_str());

    // @TODO: Maybe firewall rules
    FileServer file_server;
    file_server
            .SetConfig(_config)
            .SetLogger(logger)
            .SetHost(f_sock_addr)
            .SetPort(std::stoi(f_sock_port));

    boost::thread* fs_thread = new boost::thread(boost::bind(&FileServer::Start, &file_server));
    fs_thread->detach();

    logger.Debug("FPanel Agent starting %s:%s", sock_addr.c_str(), sock_port.c_str());
    TcpServer tcp_server;
    tcp_server.SetConfig(_config)
            .SetCommands(_commands)
            .SetLogger(logger)
            .SetHost(sock_addr)
            .SetPort(std::stoi(sock_port))
            .Start();
}

int main(int argc, char** argv)
{
    Start(argc, argv);
    return 0;
}