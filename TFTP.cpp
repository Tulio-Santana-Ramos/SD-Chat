#include "TFTP.h"
#include <iostream>
#include <bitset>
#include <cstddef>
#include <bits/stdc++.h>


#define CHUNK_SIZE 511

bool to_int(const unsigned char* c, unsigned short* n)
{
    if(c == NULL || n == NULL) return false;
    *n = (c[0] << 8) + c[1];
    return true;
}

bool to_str(unsigned char* c, const unsigned short* n)
{
    if(c == NULL || n == NULL) return false;
    c[0] = ((*n) & 65280) >> 8;
    c[1] = ((*n) & 255);
    return true;
}

std::map<int,bool> create_file_map(long long int size)
{
    int s = size/CHUNK_SIZE;
    if(size % CHUNK_SIZE > 0)
        s++;
    
    std::map<int,bool> m;
    for (int i = 0; i < s; i++)
    {
        m[i] = false;
    }
    return m;
}

bool create_header(int op,std::string file,long long int file_size, unsigned char* header)
{
    char str[255];
    unsigned char OP = op + '0';
    const char* mode = "netascii\0";
    
    sprintf(str, "%lld", file_size);
    const char* tsize_opt = "tsize\0";
    
    size_t header_size = 1+strlen(file.c_str())+1+strlen(mode)+1+strlen(tsize_opt)+1+strlen(str)+1+1;
    memset(header,0,header_size+2);

    size_t posicao = 0;
    header[posicao++] = OP;
    
    for(size_t i = 0; i < strlen(file.c_str()); i++)
    {
        header[posicao++] = file[i];
    }
    posicao++;
    
    for(size_t i = 0; i < strlen(mode); i++)
    {
        header[posicao++] = mode[i];
    }
    posicao++;

    for(size_t i = 0; i < strlen(tsize_opt); i++)
    {
        header[posicao++] = tsize_opt[i];
    }
    posicao++;

    for(size_t i = 0; i < strlen(str); i++)
    {
        header[posicao++] = str[i];
    }
    posicao++;
    
    return true;
}

size_t header_len(const unsigned char* header)
{
    bool cont = true;
    size_t posicao = 0;
    int contador = 0;
    while (cont)
    {
        unsigned char ch = header[posicao++];
        if (ch == 0) 
        {
            contador++;
        }
        else contador = 0;
        if (contador == 2) cont = false;
    }
    return posicao;
}

void print_header(const unsigned char* header)
{
    if(check_op(header) != TFTP_OPCODE_RRQ && check_op(header) != TFTP_OPCODE_WRQ) 
    {
        std::cout << "not a header\n";
        return;
    }
    bool cont = true;
    size_t posicao = 0;
    int contador = 0;
    while (cont)
    {
        char ch = header[posicao++];
        if (ch == 0) 
        {
            std::cout << "\\0";
            contador++;
        }
        else contador = 0;
        if (contador == 2) cont = false;
        std::cout << ch;
    }
    std::cout << '\n';
}

void create_ack_string(unsigned short block, unsigned char* ack)
{
    ack[0] =  TFTP_OPCODE_ACK + '0';
    to_str(&(ack[1]),&block);
    ack[3] = 0;
    return;
}

// Block from 1~n
bool create_data_block(unsigned short block, long long int file_size, std::fstream& file,  unsigned char* data)
{
    file.seekg((block-1)*CHUNK_SIZE,std::ifstream::beg);
    memset(data,0,4+CHUNK_SIZE);

    if(file.fail()) 
    {
        std::cout << "erro ao abrir arquivo\n";
        data[0] = TFTP_OPCODE_ERR + '0';
        return false;
    }
    
    data[0] = TFTP_OPCODE_DATA + '0';
    to_str(&(data[1]),&block);
    
    unsigned char ch;
    size_t i;
    for (i = 0; i < CHUNK_SIZE; i++)
    {
        if(file.eof() || file.fail() || file.tellg() == file_size) break;
        file >> std::noskipws >> ch;
        data[i+3] = ch;
    }
    return true;
}

bool get_data(const unsigned char* msg,TFTP_Data* data)
{
    if(msg == NULL || data == NULL || check_op(msg) != TFTP_OPCODE_DATA) return false;
    memset(data->data,0,512);
    data->data_size = 0;
    data->op = msg[0] - '0';
    to_int(&(msg[1]), &(data->block));
    int i = 3;
    while (i < 516)
    {
        data->data[i-3] = msg[i];
        i++;
    }
    data->data_size = i;
    return true;
}

bool write_chunk(unsigned char* buffer, TFTP_Data data, TFTP_Header header)
{
    if(data.op != TFTP_OPCODE_DATA) return false;
    for (size_t i = (data.block-1)*CHUNK_SIZE, j = 0; i < (data.block-1)*CHUNK_SIZE+CHUNK_SIZE && j < data.data_size; i++, j++)
    {
        if(i == header.file_size) break;
        buffer[i] = data.data[j];
    }
    return true;
}

std::ifstream::pos_type filesize(const std::string filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg(); 
}

int check_op(const unsigned char* msg)
{
    return msg[0]-'0';
}

// int decode_message(const char* msg);
bool decode_header(const unsigned char* msg, TFTP_Header* header)
{
    if(check_op(msg) > 2 ) return false;
/*
        estrutura do header:

          1 byte     string    1 byte    string    1 byte  string  1 byte
        -------------------------------------------------------------------
        | Opcode |  Filename  |   0   |   Mode   |   0   | t_size |   0   | 
        -------------------------------------------------------------------
*/

    char str[255];
    bool cont = true;
    size_t posicao = 0;

    // Opcode
    header->op = msg[posicao++]-'0';

    // file name
    header->file.clear();
    while (cont)
    {
        header->file.push_back(msg[posicao]);
        if(msg[posicao++] == 0) cont = false;
    }
    if(msg[posicao] == 0) posicao++;
    
    // mode
    cont = true;
    while (cont)
    {
        if(msg[posicao++] == 0) cont = false;
    }
    if(msg[posicao] == 0) posicao++;
    
    // opt
    cont = true;
    while (cont)
    {
        if(msg[posicao++] == 0) cont = false;
    }
    if(msg[posicao] == 0) posicao++;
    
    //size
    int i = 0;
    cont = true;
    while (cont)
    {
        str[i++] = msg[posicao];
        if(msg[posicao++] == 0) cont = false;
    }

    header->file_size = atoll(str);

    if(header->file_size % CHUNK_SIZE == 0) header->chunks = 0;
    else  header->chunks = 1;
    header->chunks += header->file_size/CHUNK_SIZE;

    return true;
}

bool ack_sucess(const unsigned char* msg, unsigned short* block)
{
    if(check_op(msg) != TFTP_OPCODE_ACK) return false;
    to_int(&(msg[1]),block);
    return true;
}

size_t err_len(const unsigned char* msg)
{
    /*
             1 byte     2 bytes      string    1 byte
             -----------------------------------------
            | Opcode |  ErrorCode |   ErrMsg   |   0  |
             -----------------------------------------
    */
    size_t i = 3;
    while (msg[i++] != 0);
    return i;    
}

size_t msg_len(const unsigned char* msg)
{
    switch (check_op(msg))
    {
    case TFTP_OPCODE_RRQ:
    case TFTP_OPCODE_WRQ:
        return header_len(msg);
        break;
    case TFTP_OPCODE_DATA:
    /*
     1 byte      2 bytes    512 bytes
    ----------------------------------
    | Opcode |   Block #  |   Data     |
    ----------------------------------
    */
        return 515;
        break;
    case TFTP_OPCODE_ACK:
        return 4;
        break;
    case TFTP_OPCODE_ERR:
        return err_len(msg);
        break;
    
    default:
        return 0;
        break;
    }    
    return 0;
}

void print_data_info(const unsigned char* msg)
{
    TFTP_Data data;
    get_data(msg,&data);
    std::cout << "Op:" << data.op << "\nblock: " << data.block << "\nsize: " << data.data_size << "\n";
    return;
}
