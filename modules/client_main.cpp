#include <stdexcept>
#include <sys/stat.h>
#include <cstdlib>
#include "common.h"
#include "Message.h"
#include "utils.h"

//Array of hostname of VMs
std::string vm_hosts[NUM_VMS] = {
	"fa17-cs425-g13-01.cs.illinois.edu",
	"fa17-cs425-g13-02.cs.illinois.edu",
	"fa17-cs425-g13-03.cs.illinois.edu",
	"fa17-cs425-g13-04.cs.illinois.edu",
	"fa17-cs425-g13-05.cs.illinois.edu",
	"fa17-cs425-g13-06.cs.illinois.edu",
	"fa17-cs425-g13-07.cs.illinois.edu",
	"fa17-cs425-g13-08.cs.illinois.edu",
	"fa17-cs425-g13-09.cs.illinois.edu",
	"fa17-cs425-g13-10.cs.illinois.edu"
};

//Array of unit test commands
string test_commands[NUM_TESTS] = {
	"grep hello",			//Test for infrequent pattern
	"grep -c -i r*",		//Test for multiple flags and regex
	"grep -i gET",			//Test for -i flag (ignore Case)
	"grep -n test",			//Test for -n flag (show line number)
	"grep asdasd",			//Test for non-exist pattern
	"grep -v dsa",			//Test for -v flag (invert match)
	"grep 2$",				//Test for finding lines ending with Pattern
	"grep ^1",				//Test for finding lines starting with Pattern
	"grep GET",				//Test for frequent pattern
};


using namespace std;
using namespace logging;
int do_grep_local(string input_cmd, int my_id);
void writeToFile(vector<string>& vmi_result, int i);
void check_result(int num_tests);

int main(int argc, char ** argv) {
	bool unit_test = false;	 //Flag for unit test
	int test_num = 0;		   //Test number
	//Check if running unit test or not
	if(argc == 2 && strcmp(argv[1], "unit_test") == 0){
		unit_test = true;
		test_num = 0;
	}

	while(1){
		int socket_fds[NUM_VMS];				//Array of socket fs
		int num_alive = 0;					  //Number of connected VMs
		vector<vector<string> > results;		//Results of grep from all VMs
		int results_count[NUM_VMS] = {0};	   //Number of lines found from grepping each VM
		bool failed[NUM_VMS] = {true};		  //Array of unconnected VMs
		int sock_fd;							//Socket fd of this VM
		char buf[BUF_SIZE];					 //Buffer to store msg received from other VMs
		fd_set r_master, r_fds;			//set of fds
		int sock_to_vm[NUM_VMS] = {-1};		 //Array to convert socket fd to VM id
		bool sent_request[NUM_VMS] = {false};   //Array of Vms that already sent request
		int receive_order[NUM_VMS] = {-1};	  //Array to store order of receiving msg
		int max_fd = -1;						//Highest fd among socket fds
		timepnt begin;							// The time at which we request a grep
		string cmd_str;
		
		if(unit_test == true){
			//Get command from test commands array
			cmd_str = test_commands[test_num];
		}
		else{
			//Wait for user input
			cout << "prompt>";
			getline(cin, cmd_str);
		}

		//Start timer
		begin = clk::now();
		
		//Check if user want to quit
		if(cmd_str == "quit")
			break;
		
		//Get id of this VM
		char my_addr[HOSTNAME_LENGTH];
		int my_id = -1;
		gethostname(my_addr,HOSTNAME_LENGTH);
		vm_str_num(&my_addr[15], &my_id);
		my_id--;
		
		//Reset all fd set
		FD_ZERO(&r_master);
		FD_ZERO(&r_fds);

		//Make connection to all VMS
		for(int i = 0 ; i < NUM_VMS; i++){
			if(i == my_id){
				continue;
			}
			struct addrinfo hints, *ai;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			try {
				handle_qerror(getaddrinfo(vm_hosts[i].c_str(), PORT_STR, &hints, &ai) == -1, "Failed getting address information for %d", i);
				handle_perror((sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1, "Socket creation for %d failed.\n", i);
				handle_perror(connect(sock_fd, ai->ai_addr, ai->ai_addrlen) == -1, "Connect to %d failed.", i);
				if(sock_fd != -1) {	  //Can connect to Vmi
					socket_fds[i] = sock_fd;
					failed[i] = false;
					num_alive ++;
					sock_to_vm[sock_fd] = i;
					FD_SET(socket_fds[i], &r_master);
					max_fd = max_fd > socket_fds[i] ? max_fd : socket_fds[i];
								//Send request
					Message cmd_msg(cmd_str.size(), cmd_str.c_str());
					if(cmd_msg.send_msg(sock_fd) == -1){
						perror("Client: send");
					}
				}
			} catch(runtime_error & r) {
				sock_fd = -1;
				continue;
			}
		}

		//Do grep on local machine
		results_count[my_id] = do_grep_local(cmd_str, my_id);

		//Loop to send request and wait for other VMs to response
		while(results.size() <(unsigned int) num_alive ) {
			//break when Timeout
			if(std::chrono::duration_cast<unit_milliseconds>(clk::now() - begin).count() > TIMEOUT){
				break;
			}
			r_fds = r_master;
			if(select(max_fd+1, &r_fds, NULL, NULL, NULL) == -1){
				perror("client: select");
				exit(4);
			}
			//Go through all fds to check if can read or write from which fd
			for(int i = 1 ; i <= max_fd; i++){
				if(FD_ISSET(i, &r_fds)){	// There is a fd ready to read
					int nbytes = 0;		 // Number received bytes
					int line_count;		 // Number of lines found from grepping
					Message my_msg;
				
					if(recv(i, &line_count,sizeof(int), 0) <=0){
						//There is error or connection is close
						close(i);
						FD_CLR(i, &r_master);
					}
					else{
						line_count = ntohl(line_count); //Convert from network byte order to host byte order
						int length = my_msg.receive_int_msg(i);
						int temp = 0;
						vector<string> temp_results;
						//Loop to receive and store msg
						while(1 && length!=0){
							if((nbytes = (int)recv(i, buf, sizeof(buf), 0))  <= 0){
								if(nbytes <0){
									perror("client: recv");
								}
								else{
									cout << "client: socket " << i << "hung up\n";
								}
								break;
							}
							else{
								temp += nbytes;
								string temp_str(buf,nbytes);
								temp_results.push_back(temp_str);
								if(temp >= length)
									break;
							}
						}
						//Close connection and remove fd from read fd set
						close(i);
						FD_CLR(i, &r_master);
						//Store result
						receive_order[sock_to_vm[i]] = results.size();
						results_count[sock_to_vm[i]] = line_count;
						results.push_back(temp_results);
					}

				}
			}
		}

		//Write result to file
		for(int i = 0; i < NUM_VMS; i++){
			if(results_count[i] >0 && receive_order[i] >=0){
				vector<string> vmi_result = results[receive_order[i]];
				writeToFile(vmi_result, i);
			} else if(i != my_id) {
				vector<string> vmi_result;
				writeToFile(vmi_result, i);
			}
		}
		int total = 0;
		//Print out number of lines found from each VM and Calculate number of total lines
		for(int i = 0 ; i<NUM_VMS; i++){
			if(results_count[i] > 0){
				cout <<"VM" <<i+1 << " found: " << results_count[i] << " lines.\n";
			}
			total += results_count[i];
		}
		cout << "Totally found: " << total << " lines.\n";
		
		//Print out the latency time
		cout << "Time Taken: " << std::chrono::duration_cast<unit_milliseconds>(clk::now() - begin).count() <<"ms"<< endl;

		//Check the results if this is a unit test
		if(unit_test == true){
			check_result(test_num);
			test_num++;
			if(test_num == NUM_TESTS){
				break;
			}
		}
	}
	return 0;
}

/**
 * Compares each vm's output with the given solution.
 * Input:   test_num:   test number
 * Return:  None
 */
void check_result(int test_num){
	bool success =true;						 //Flag to indicate result
	bool fail_array[NUM_TEST_VMS] = {false};   //Array of tested VMs

	cout << "Command: " << test_commands[test_num] << "\n";
	
	//Loop through all Files and check the result
	for(int i = 1; i <= NUM_TEST_VMS; i++){
		//Get name of output files
		string output_name("out/outputVM");
		output_name += (char)(i + '0');
		output_name += ".txt";
		
		//Get name of result files
		string test_result_name("Tests/Test");
		test_result_name += (char)(test_num + '1');
		test_result_name += "/vm";
		test_result_name += (char)(i + '0');
		test_result_name += "_out.txt";
		
		//Execute diff command between 2 files
		FILE* file;
		string cmd("diff ");
		cmd += output_name;
		cmd += " ";
		cmd += test_result_name;
		handle_qerror((file = popen(cmd.c_str(), "r")) == 0, "Popen failed for %d\n", i);
		char output[10];
		memset(output, 0, 10);
		fgets(output, 10, file);
		
		//Check the output of diff command
		if(output[0] != '\0') {
			success = false;
			fail_array[i-1] = true;
		}
		pclose(file);
	}
	
	if(success == true){
		//Print out test result
		cout << "------------Test " << test_num +1<< " successful.---------------" << endl;
	}
	else{
		//Print out number of failed VM
		cout << "Test " << test_num+1 << " failed for VM: ";
		for(int i = 0 ; i < NUM_TEST_VMS; i++){
			if(fail_array[i] == true)
				cout << i+1 <<" ";
		}
		cout <<"\n";
	}
	
	return;
}



/*
 This function do grep on local machine and write the result to file
 Argument:  input_cmd:  Command from user 
			my_id:	  id of current VM
 Return:	Number of lines found from doing grep in this VM
 */
int do_grep_local(string input_cmd, int my_id){
	FILE* file;			 //File pointer to popen
	int count = 0;		  //Number of lines found
	ostringstream stm ;	 //Output string stream
	char line[MAX_LINE_SZ]; //Buffer to store result from grep
	string result;		  //Result from doign grep
	ofstream out_file;	  //


	//Calculate cmd from user input and local VM id
	string cmd = input_cmd + " " + "out/vm" + (char)(my_id+'1') + ".log";
	if(!(file = popen(cmd.c_str(), "r"))){
		return 0;
	}
	
	//Read and store result in stm and count number of lines found
	while(fgets(line, MAX_LINE_SZ, file)){
		stm << line;
		count++;
	}
	pclose(file);
	result = stm.str();
	
	//Store result to output file
	string name("out/outputVM");
	name += (char)(my_id+'1');
	name += ".txt";
	out_file.open (name.c_str());
	out_file << result;
	out_file.close();
	return count;
}

/*
 This function write input to a file
 Arguments: vmi_result: Array of result found from doing grep on VM
			i:  id of Vm that the result come from
 */
void writeToFile(vector<string>& vmi_result, int i){
	ofstream file;
	//result from VM-i is stored in file output_VMi.txt
	string name("out/outputVM");
	if(i <9){ 
	name += (char)(i+'1');
	}
	else{
	name += "10";
	}
	name += ".txt";
	
	file.open (name.c_str());
	for(int j = 0; j < (int)vmi_result.size(); j ++){
		file << vmi_result[j];
	}
	file.close();
}
