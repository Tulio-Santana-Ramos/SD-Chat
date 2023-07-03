#include <string>
#include <map>
#include <fstream>

/*
    Tamanho limite de transferencia atual: 16.743.937 bytes (16 Mb) devido ao numero de blocos estarem limitados a 2 bytes (32767 blocos)
*/


#define TFTP_OPCODE_RRQ 1
#define TFTP_OPCODE_WRQ 2
#define TFTP_OPCODE_DATA 3
#define TFTP_OPCODE_ACK 4
#define TFTP_OPCODE_ERR 5

typedef struct {
    int op;
    std::string file;
    long long int file_size;
    unsigned short chunks;
} TFTP_Header;

typedef struct {
    int op;
    unsigned char data[512];
    int data_size;
    unsigned short block;
} TFTP_Data;

std::ifstream::pos_type filesize(const std::string filename);

std::map<int,bool> create_file_map(long long int size);
void print_map(std::string_view comment, const std::map<int, bool>& m);
// std::string create_msg(int op);
// void print_msg(std::string msg);
bool get_data(const unsigned char* msg, TFTP_Data *data);
bool create_data_block(unsigned short block, long long int file_size, std::fstream& file, unsigned char* msg);

bool create_header(int op,std::string file,long long int file_size, unsigned char* header);
size_t header_len(const unsigned char* header);
void print_header(const unsigned char* header);

int check_op(const unsigned char* msg);
int decode_message(const unsigned char* msg);
bool decode_header(const unsigned char* msg, TFTP_Header* header);
bool ack_sucess(const unsigned char* msg, unsigned short* block);
// std::string create_ack_string(int block);
void create_ack_string(unsigned short block, unsigned char* ack);
// char *create_ack_string(int block);

bool write_chunk(unsigned char* buffer, TFTP_Data data, TFTP_Header header);
size_t msg_len(const unsigned char* msg);

void print_data_info(const unsigned char* msg);