//============================================================================
// Name        : modem_async_server.cpp
// Author      : First
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <ctime>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <istream>
#include <ostream>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <mysql.h>
#include <mysql++.h>
#include <math.h>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/crc.hpp>
#include <boost/spirit/include/qi_binary.hpp>
#include <algorithm>







#pragma pack(push,1)
struct packt {//paket of modbus

  unsigned char id;//number of device

  unsigned char fu;//function code, what part will need to read

  /////bytes that send how much we need to read
  unsigned char dat1;
  unsigned char dat2;
  unsigned char dat3;
  unsigned char dat4;
  ///

  unsigned int  crc1:16;//for checksum CRC16


};

#pragma pack(pop)

#pragma modbus_packet_serial(push,1) // modbus packet for get device serial number
struct modbus_packet_serial {//paket of modbus

  unsigned char rand;//number of device

  unsigned char fu;//function code, what part will need to read

  unsigned int  crc1:16;//for checksum CRC16


};

#pragma modbus_packet_serial(pop)


MYSQL *conn, mysql;
MYSQL_RES *res;
MYSQL_ROW row;

int query_state;

const char *server="52.8.83.48";
const char *user="007";
const char *password="007";
const char *database="amaDB";
std::string add_data; // string with mysql insert command
std::string all_data[12]; // store data from devices
std::string water_speed_data[6]; // water data from devices

//boost::array<char, 32> recv_buf;
packt P1; // declaration of modbus packet for get data from device
packt rq_modbus; // declaration of modbus packet for get data from RQ-30 device
modbus_packet_serial serial_number; // declaration of modbus packet for get serial number from device
char *water_level;
char *discharge;
char *water_speed;
std::string modem_command_crc = "#W0001$mt|BE85;"; // check RQ-30 availability
std::string modem_command_data = "#S0001$pt|;"; // get data RQ-30
std::string modbus_serial_data_string = ""; // store modbus serial answer in string
char modbus_serial_data[64]; // store modbus serial answer
char modbus_data[64]; // store modbus device parameters answer
char str_ack_data[64]; // RQ-30 availability answer
char rq_modbus_data[64]; // RQ-30 data answer
std::string str1("|02"); // key for find level data
std::string str3("|05"); // key for find discharge data
std::string data; // modbus device parameters answer converted to string
std::string level; // store level data from RQ-30
//std::string discharge; // store discharge data form RQ-30
double data_level = 0; // variable store converted level data (19 -> 1.9)
double data_discharge = 0; // variable store converted discharge data (19 -> 1.9)
int i = 0; // use for parse data from RQ-30
uint32_t num;
float f;
bool data_not_receive[6] = {true, true, true, true, true, true};
size_t attempt_num = 0;
size_t modem_num = 0;




std::string char_int_hex(unsigned char t1)
{
	std::stringstream stream;
	stream << std::setfill('0') <<std:: setw(2) <<std:: hex << t1 + 0;
	return stream.str();
}

std::string int_to_hex_char(char t1)
{
	std::stringstream stream;
	stream << std::setfill('0') << std::setw(2) << std::hex << int(static_cast<unsigned char>(t1)) + 0;
	return stream.str();
}

int GetCrc16(const std::string& my_string) {//GET CRC 16 checksumm
	boost::crc_optimal<16, 0x8005, 0xFFFF, 0, true, true> result;//configurating CRC boost function for Modbus
	result.process_bytes(my_string.data(), my_string.length());//put string ( that we need to get check summ from ) and its length
	return result.checksum();

	/*According to the manual, checksum() returns "the CRC checksum of the data passed in so far".
	So, the second checksum is the checksum of the concatenation of data1 with itself and thus naturally different from the checksum of data1.*/

}



void StartAccept( boost::asio::ip::tcp::acceptor& );

void ServerThreadFunc( boost::asio::io_service& io_service )
{
  using boost::asio::ip::tcp;
  tcp::acceptor acceptor( io_service, tcp::endpoint( tcp::v4(), 6572 ) );

  // Add a job to start accepting connections.
  StartAccept( acceptor );

  // Process event loop.
  io_service.run();

  std::cout << "Server thread exiting." << std::endl;
}



// modbus data
// ************************************************************************************************
void handle_process_modbus_data(const boost::system::error_code& error, boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
	  if ( error )
	  {
	    std::cout << "Errorrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr: " << error.message()
	              << std::endl;
	  }

    std::cout << "Breakpoint 2: " << std::endl;
    //std::cout << recv_buf.data() << std::endl;
    for (int l = 0; l < 64; l++) {
    	std::cout<< char_int_hex(modbus_data[l]);
    }

    std::cout << std::endl;
    std::cout << "Breakpoint 3: " << std::endl;

  	std::string HexNum = "";
  	int measurement[4];
  	int iter = 3;
  	double withpoint = 0;

  	std::cout << "Modbus data: " << std::endl;
  					for (int inter = 0; inter < 4; inter++) {
  						for (int inter3 = 0; inter3 < 4; inter3++) {
  							HexNum += int_to_hex_char(modbus_data[iter++]);//считываем побайтово hex в стринг с помошью функции (используюшей поток для возвражения строки какждого октена, всего их 4)
  						}
  						measurement[inter] = strtoul(HexNum.substr(0, 8).c_str(), NULL, 16);//переводим из 16-ричной в десятичную блок из 4 октенов
  						std::cout << measurement[inter] << std::endl;
  						HexNum = "";
  						withpoint = double(measurement[inter])/10;
  						all_data[inter+2] = boost::lexical_cast<std::string>(withpoint);
  						//withpoint /= 10;
  						std::cout << withpoint << std::endl;
  					}
  										usleep(1000000);
  										using namespace std;
  										mysql_init(&mysql);
  										conn=mysql_real_connect(&mysql, server, user, password, database, 0, 0, 0);
  										if(conn==NULL)
  										{
  											cout<<mysql_error(&mysql)<<endl<<endl;
  										}

  									add_data = "INSERT INTO Dis_table (DateTime, Level, Discharge, NIT, CHL, AMM, TURB) "
  											"values (NOW(), '"+all_data[0]+"', '"+all_data[1]+"', '"+all_data[2]+"', '"+all_data[3]+"', '"+all_data[4]+"', '"+all_data[5]+"')";

  									query_state=mysql_query(conn, add_data.c_str());



  									if(query_state!=0)
  									{
  										cout<<mysql_error(conn)<<endl<<endl;
  									}
  									if(query_state!=0)
  									{
  										cout<<mysql_error(conn)<<endl<<endl;
  									}
  									mysql_free_result(res);
  									mysql_close(conn);


  					                usleep(1000000);
}

void handle_read_modbus(boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
    socket -> async_read_some( boost::asio::buffer(modbus_data), boost::bind(handle_process_modbus_data, boost::asio::placeholders::error, socket));
}


void handle_write_sleep_modbus(boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
    boost::asio::io_service io;

    boost::asio::deadline_timer t(io, boost::posix_time::seconds( 2 ));
    t.async_wait(boost::bind(handle_read_modbus, socket));
    io.run();
}
// ************************************************************************************************




// RQ modbus data
// ************************************************************************************************

void handle_process_rq_modbus_data(const boost::system::error_code& error, boost::shared_ptr< boost::asio::ip::tcp::socket > socket) {
	  if ( error )
	  {
	    std::cout << "Errorrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr: " << error.message() << std::endl;
	  }


	water_level = (char *)malloc((5)*sizeof(char));
	discharge = (char *)malloc((5)*sizeof(char));
	water_speed = (char *)malloc((5)*sizeof(char));
	memcpy(water_level, "0x", 1);
	memcpy(water_level, rq_modbus_data+3, 4);



	/*int cou = 0;
	int posit = 0;
	memcpy(water_level, rq_modbus_data, 4);
    std::reverse(water_level, water_level + 4);
    f = *reinterpret_cast<float*>(water_level);
    //if ()
    all_data[cou++] = boost::lexical_cast<std::string>(f);
	for (int h = 3; h < 41; h+=4) {
		memcpy(water_level, rq_modbus_data+h, 4);
	    std::reverse(water_level, water_level + 4);
	    f = *reinterpret_cast<float*>(water_level);
	    f *= 100;
	    all_data[cou++] = boost::lexical_cast<std::string>(f);
	    //posit = boost::lexical_cast<int>(f);

	}*/



	memcpy(discharge, "0x", 1);
	memcpy(discharge, rq_modbus_data+15, 4);

	memcpy(water_speed, "0x", 1);
	memcpy(water_speed, rq_modbus_data+7, 11);

    data_level = 0;
    data_discharge = 0;
    i = 0;
    std::cout << "rq modbus data result: " << std::endl;
    for (int l = 0; l < 64; l++) {
    	std::cout<< char_int_hex(rq_modbus_data[l]);
    }
    data = boost::lexical_cast<std::string>(rq_modbus_data);


    num = 0;
    f = 0;

    //std::cout << std::hex << water_level << std::endl;
    std::reverse(water_level, water_level + 4);
    f = *reinterpret_cast<float*>(water_level);
    if (f < 0) f = 0;
    all_data[modem_num*2] = boost::lexical_cast<std::string>(f);
    //all_data[0] = boost::lexical_cast<std::string>(f);
    std::cout << std::endl;
    std::cout << f << std::endl;

    std::reverse(discharge, discharge + 4);
    f = *reinterpret_cast<float*>(discharge);
    if (f < 0) f = 0;
    all_data[modem_num*2+1] = boost::lexical_cast<std::string>(f);
    //all_data[1] = boost::lexical_cast<std::string>(f);
    std::cout << f << std::endl;

    std::reverse(water_speed, water_speed + 4);
    f = *reinterpret_cast<float*>(water_speed);
    if (f < 0) f = 0;
    water_speed_data[modem_num] = boost::lexical_cast<std::string>(f);

    free(water_level);
    free(discharge);
    free(water_speed);


	//free(water_level);
    //if (modem_num == 0) {
    	//boost::asio::async_write(*socket, boost::asio::buffer(&P1, 8), boost::bind(handle_write_sleep_modbus, socket));
    //}
    data_not_receive[modem_num] = false;
    /*if (modbus_serial_data_string == "25130470") {
    	all_data[modem_num*2+1] = boost::lexical_cast<std::string>(f * 19.3);
    }*/


    //all_data[attempt_num-1] = modbus_serial_data_string;




// if modem has one device
// **************************************************************************************

    	//if(data_not_receive[0] == false && data_not_receive[1] == false) {
    	if (attempt_num >= 6) {
					usleep(1000000);
  	  	  	  	  	using namespace std;
					mysql_init(&mysql);
					conn=mysql_real_connect(&mysql, server, user, password, database, 0, 0, 0);
					if(conn==NULL)
					{
						cout<<mysql_error(&mysql)<<endl<<endl;
					}

					add_data = "INSERT INTO Dis_table (DateTime, Level1, Discharge1, Level2, Discharge2, Level3, Discharge3, Level4, Discharge4, Level5, Discharge5, Level6, Discharge6) "
																	"values (NOW(), '"+all_data[0]+"', '"+all_data[1]+"', '"+all_data[2]+"', '"+all_data[3]+"', '"+all_data[4]+"', '"+all_data[5]+"', '"+all_data[6]+"', '"+all_data[7]+"', '"+all_data[8]+"', '"+all_data[9]+"', '"+all_data[10]+"', '"+all_data[11]+"')";
					/*if (modbus_serial_data_string == "25130473") {
						add_data = "INSERT INTO Dis_table (DateTime, Level, Discharge, NIT, CHL, AMM, TURB) "
												"values (NOW(), '"+all_data[0]+"', '"+all_data[1]+"', '"+all_data[2]+"', '"+all_data[3]+"', '"+all_data[4]+"', '"+all_data[5]+"')";
					} else {
						add_data = "INSERT INTO Dis_table (DateTime, Level, Discharge, NIT, CHL, AMM, TURB) "
												"values (NOW(), '"+all_data[2]+"', '"+all_data[3]+"', '"+all_data[0]+"', '"+all_data[1]+"', '"+all_data[4]+"', '"+all_data[5]+"')";
					}*/


					query_state=mysql_query(conn, add_data.c_str());



					add_data = "INSERT INTO velocity (DateTime, velocity1, velocity2, velocity3, velocity4, velocity5, velocity6) "
							   "values (NOW(), '"+water_speed_data[0]+"', '"+water_speed_data[1]+"', '"+water_speed_data[2]+"', '"+water_speed_data[3]+"', '"+water_speed_data[4]+"', '"+water_speed_data[5]+"')";

					query_state=mysql_query(conn, add_data.c_str());

					if(query_state!=0)
					{
						cout<<mysql_error(conn)<<endl<<endl;
					}
					if(query_state!=0)
					{
						cout<<mysql_error(conn)<<endl<<endl;
					}
					mysql_free_result(res);
					mysql_close(conn);

					data_not_receive[0] = true;
					data_not_receive[1] = true;
					data_not_receive[2] = true;
					data_not_receive[3] = true;
					data_not_receive[4] = true;
					data_not_receive[5] = true;
					attempt_num = 0;
					usleep(1000000);
    }

// ************************************************************************************************
}

void handle_rq_modbus_data(boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
    socket -> async_read_some( boost::asio::buffer(rq_modbus_data), boost::bind(handle_process_rq_modbus_data, boost::asio::placeholders::error, socket));
}

void handle_sleep_rq_modbus_data(boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
    boost::asio::io_service io;
    std::cout << "rq modbus data result: " << std::endl;
    boost::asio::deadline_timer t(io, boost::posix_time::seconds( 2 ));
    t.async_wait(boost::bind(handle_rq_modbus_data, socket));
    io.run();
}



// ************************************************************************************************



// modbus serial data
// ************************************************************************************************

void handle_process_modbus_serial_data(boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
	add_data = "";

  	std::string HexNum = "";
  	int iter = 32;
  	std::cout << "Modbus serial data: " << std::endl;
  						for (int inter3 = 0; inter3 < 8; inter3++) {
  							HexNum += int_to_hex_char(modbus_serial_data[iter++]);//считываем побайтово hex в стринг с помошью функции (используюшей поток для возвражения строки какждого октена, всего их 4)
  						}
  						std::cout << HexNum << std::endl;


  						int len = 16;
  						std::string newString;
  						for(int i=0; i< len; i+=2)
  						{
  						    std::string byte = HexNum.substr(i,2);
  						    char chr = (char) (int)strtol(byte.c_str(), NULL, 16);
  						    newString.push_back(chr);
  						}
  						std::cout << "serial number: " << newString << std::endl;
  						std::cout << "Modbus: " << std::endl;
  						modbus_serial_data_string = newString;


  						if (modbus_serial_data_string == "25130473") {
  							modem_num = 0;
  						} else if (modbus_serial_data_string == "25130470") {
  							modem_num = 1;
  						} else if (modbus_serial_data_string == "25130480") {
  							modem_num = 2;
  						} else if (modbus_serial_data_string == "25130478") {
  							modem_num = 3;
  						} else if (modbus_serial_data_string == "25130483") {
  							modem_num = 4;
  						} else if (modbus_serial_data_string == "25130479") {
  							modem_num = 5;
  						}
  						attempt_num++;
  						//if (data_not_receive[modem_num]) {
  						//if (modbus_serial_data_string == "25130470") {
  							boost::asio::async_write(*socket, boost::asio::buffer(&rq_modbus, 8), boost::bind(handle_sleep_rq_modbus_data, socket));
  						//}
}

void handle_read_modbus_serial(boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
    socket -> async_read_some( boost::asio::buffer(modbus_serial_data), boost::bind(handle_process_modbus_serial_data, socket));
}


void handle_write_sleep_modbus_serial(boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
    boost::asio::io_service io;

    boost::asio::deadline_timer t(io, boost::posix_time::seconds( 2 ));
    t.async_wait(boost::bind(handle_read_modbus_serial, socket));
    io.run();
}

void handle_write(const boost::system::error_code& error,
	      size_t /*bytes_transferred*/)
{
	  if ( error )
	  {
	    std::cout << "Error accepting connection: " << error.message()
	              << std::endl;
	    return;
	  }
}
// ************************************************************************************************



void print(boost::shared_ptr< boost::asio::ip::tcp::socket > socket)
{
	  char buff[128];
	  //socket -> async_read_some( boost::asio::buffer(buff), handle_read);
	  std::cout << buff << std::endl;
}



void handler(
  const boost::system::error_code& error // Result of operation.
) {};

void HandleAccept( const boost::system::error_code& error,
                   boost::shared_ptr< boost::asio::ip::tcp::socket > socket,
                   boost::asio::ip::tcp::acceptor& acceptor )
{
  // If there was an error, then do not add any more jobs to the service.
  if ( error )
  {
    std::cout << "Error accepting connection: " << error.message()
              << std::endl;
    return;
  }
  std::cout << "Modbus serial number: " << std::endl;
  boost::asio::async_write(*socket, boost::asio::buffer(&serial_number, 4), boost::bind(handle_write_sleep_modbus_serial, socket));

  //usleep(30000000);


  // Perform async operations on the socket.

  // Done using the socket, so start accepting another connection.  This
  // will add a job to the service, preventing io_service::run() from
  // returning.
  std::cout << "Done using socket, ready for another connection."
            << std::endl;
  StartAccept( acceptor );
};

void StartAccept( boost::asio::ip::tcp::acceptor& acceptor )
{
  using boost::asio::ip::tcp;
  boost::shared_ptr< tcp::socket > socket(
                                new tcp::socket( acceptor.get_io_service() ) );

  // Add an accept call to the service.  This will prevent io_service::run()
  // from returning.
  std::cout << "Waiting on connection" << std::endl;
  acceptor.async_accept( *socket,
    boost::bind( HandleAccept,
      boost::asio::placeholders::error,
      socket,
      boost::ref( acceptor ) ) );
}

int main()
{
  using boost::asio::ip::tcp;

  	// modbus
  	// ************************************************************************************************
	//unsigned char input;
	char *packet;
	packet = new char[2];

	int modbusID = 1;
	//unsigned char p = static_cast<unsigned char>(modbusID);
	int modbusFUN = 4;
	//int e = 0;
	//unsigned char p2 = static_cast<unsigned char>(modbusFUN);
	//unsigned char *pa = new unsigned char[3];
	std::string CRC;
	P1.id = modbusID;
	P1.fu = modbusFUN;
	P1.dat1 = 0;
	P1.dat2 = 20;
	P1.dat3 = 0;
	P1.dat4 = 8;
	unsigned char *dats;
		dats = new unsigned char[6];
	memcpy(dats, &P1, 6);

	CRC = P1.id;
	CRC += P1.fu;
	CRC += P1.dat1;
	CRC += P1.dat2;
	CRC += P1.dat3;
	CRC += P1.dat4;

	unsigned short checkSUM16_Modbus = GetCrc16(CRC);
	P1.crc1 = checkSUM16_Modbus;
	// ************************************************************************************************

	// modbus rq-30
	// ************************************************************************************************

	std::string CRC_RQ;
	rq_modbus.id = 2;
	rq_modbus.fu = 4;
	rq_modbus.dat1 = 0;
	rq_modbus.dat2 = 4;
	rq_modbus.dat3 = 0;
	rq_modbus.dat4 = 12;

	CRC_RQ = rq_modbus.id;
	CRC_RQ += rq_modbus.fu;
	CRC_RQ += rq_modbus.dat1;
	CRC_RQ += rq_modbus.dat2;
	CRC_RQ += rq_modbus.dat3;
	CRC_RQ += rq_modbus.dat4;

	unsigned short checkSUM16_Modbus_RQ = GetCrc16(CRC_RQ);
	rq_modbus.crc1 = checkSUM16_Modbus_RQ;
	// ************************************************************************************************

	// modbus serial rq-30
	// ************************************************************************************************
	std::string CRC2;
	serial_number.rand = 2;
	serial_number.fu = 0x11;
	CRC2 = serial_number.rand;
	CRC2 += serial_number.fu;
	unsigned short checkSUM16_Modbus3 = GetCrc16(CRC2);
	serial_number.crc1 = checkSUM16_Modbus3;
	//df.fu = 0x11;
	std::cout << serial_number.crc1 << std::endl;
    // ************************************************************************************************


  // Create io service.
  boost::asio::io_service io_service;

  // Create server thread that will start accepting connections.
  boost::thread server_thread( ServerThreadFunc, boost::ref( io_service ) );

  // Sleep for 30 seconds, then shutdown the server.
  std::cout << "Stopping service in 1 year..." << std::endl;
  boost::this_thread::sleep( boost::posix_time::hours( 365*24 ) );
  std::cout << "Stopping service now!" << std::endl;

  // Stopping the io_service is a non-blocking call.  The threads that are
  // blocked on io_service::run() will try to return as soon as possible, but
  // they may still be in the middle of a handler.  Thus, perform a join on
  // the server thread to guarantee a block occurs.
  io_service.stop();

  std::cout << "Waiting on server thread..." << std::endl;
  server_thread.join();
  std::cout << "Done waiting on server thread." << std::endl;

  return 0;
}
