#include "web_server.h"
#include "network_low.h"
#include "generate_json.h"
#include "power_counting.h"
#include <stdio.h>
#include "socket.h"
#include <string.h>

#include "index.h"
#include "zepto_gzip.h"
#include <stdlib.h>

const char http_404_full[] =
	"HTTP/1.0 404 Not Found\r\n"
	"Content-Type: text/html;"
	"Server: STM32+W5500\r\n"
	"\r\n"
	"<pre>Page not found\r\n\r\n";


#define WEB_DATA_BUF_SIZE       2048
uint8_t web_data_buf[WEB_DATA_BUF_SIZE];
#define SOCK_WEB_CNT            3//число сокетов //SOCKET_CODE
#define SOCK_WEB_PORT           80//Порт web страницы


const char  http_200[] = "HTTP/1.0 200 OK\r\n";
const char http_server[] = "Server: STM32+W5500\r\n";
const char http_connection_close[] = "Connection: close\r\n";
const char http_content_type[] = "Content-Type: ";
const char http_content_length[] = "Content-Length: ";
const char http_content_encoding[] = "Content-Encoding: gzip\r\n";
const char http_linebreak[] = "\r\n";
const char http_header_end[] = "\r\n\r\n";

const char http_text_html[] = "text/html";
const char http_text_js[] = "text/javascript";
const char http_cgi[] = "application/cgi";

char default_page[]="index.html";

uint32_t sentsize[SOCK_WEB_CNT+1];
uint8_t http_state[SOCK_WEB_CNT+1];
uint8_t http_url[SOCK_WEB_CNT+1][25];

extern TypeEthState ethernet_state;

int decode_unicode(const char* s, char* dec);

void web_server_handler(void)
{
  uint8_t i;
  if (ethernet_state != ETH_STATE_GOT_IP)
  {
    //no ip
  }
  else
  {
    for (i=1; i < (SOCK_WEB_CNT + 1); i++)//перебираем сокеты
    {
      if(loopback_web_server(i, web_data_buf, SOCK_WEB_PORT) < 0) 
      {
#ifdef DEBUG
	printf("SOCKET ERROR\r\n");
#endif
      }
    }//end of for
  }//end of else ETH_STATE_GOT_IP
}



// get mime type from filename extension
const char *httpd_get_mime_type(char *url)
{
  const char *t_type;
  char *ext;
  
  t_type = http_text_html;
  
  if((ext = strrchr(url, '.')))
  {
    ext++;
    //strlwr(ext);
    if(strcmp(ext, "htm")==0)       
      t_type = http_text_html;
    else if(strcmp(ext, "html")==0) 
      t_type = http_text_html;
    else if(strcmp(ext, "js")==0)   
      t_type = http_text_js;
    else if(strcmp(ext, "cgi")==0)  
      t_type = http_cgi;
  }
  
  return t_type;
}

// Сброс состояния обработчика http для сокета "sock_num"
void web_http_reset(uint8_t sock_num)
{
  sentsize[sock_num] = 0;
  http_state[sock_num] = HTTP_IDLE;
  memset(&http_url[sock_num][0], 0, 25);//clear url
}

//выполняет действия в зависимости от состояния заданного сокета
//sock_num - номер сокета
//buf - буфер для работы с сокетом
//port - порт
int32_t loopback_web_server(uint8_t sock_num, uint8_t* buf, uint16_t port)
{
   int32_t ret;
   uint32_t rx_size = 0;
   char *url, *p, str[32];
   const char *mime;
   
   uint16_t header_sz = 0;
   
   uint32_t file_size = 0;
   uint16_t bytes_read = 0;
  
  switch(getSn_SR(sock_num))
  {
    case SOCK_ESTABLISHED :
      if(getSn_IR(sock_num) & Sn_IR_CON)
      {
        setSn_IR(sock_num, Sn_IR_CON);
#ifdef DEBUG
        printf("%d:Connected\r\n", sock_num);
#endif
      }
      if((rx_size = getSn_RX_RSR(sock_num)) > 0)//Received Size Register - there are some bytes received
      {
        if(rx_size > WEB_DATA_BUF_SIZE) 
          rx_size = WEB_DATA_BUF_SIZE;
        
        ret = recv(sock_num, buf, rx_size);
        web_http_reset(sock_num);
        buf[rx_size] = 0;//terminate rx string
        
        if(ret <= 0)
          return ret;
        
        url = (char*)buf + 4;// extract URL from request header
        
        if (http_state[sock_num] == HTTP_IDLE)
        {
          // **************** GET ********************************************
          if ((memcmp(buf, "GET ", 4) == 0) && (p = strchr(url, ' ')))
          {
            *(p++) = 0;//making zeroed url string
            sentsize[sock_num]=0;
            
            if(strcmp(url,"/") == 0)
              url = default_page;
            else
              url++;//иначе url будет содержать "/"
            
#ifdef DEBUG
            printf("URL : %s\r\n", url);
#endif
            
            file_size = (uint32_t)url_exists(url);
#ifdef DEBUG
            printf("FILE SIZE : %d\r\n", file_size);
#endif           
            //http data fill
            if(file_size > 0)
            {
              memcpy(&http_url[sock_num][0], url, 25);
              
              mime = httpd_get_mime_type(url);
              strcpy((char*)buf, http_200);
              
              //from here possibly not mandatory?
              strcat((char*)buf, http_server);
              strcat((char*)buf, http_connection_close);
              
              strcat((char*)buf, http_content_length);
              sprintf(str, "%d\r\n", file_size);
              strcat((char*)buf, str);
              //strcat((char*)buf, http_linebreak);//till here possibly not mandatory?
              
              if (memcmp(mime, "text/javascript", 15) == 0)
              {
                strcat((char*)buf, http_content_encoding);//use encoding
              }
              strcat((char*)buf, http_content_type);
              strcat((char*)buf, mime);
              strcat((char*)buf, http_header_end);
              
              header_sz = strlen((char*)buf);
              
              http_state[sock_num] = HTTP_SENDING;
            }
            else
            {
              //404 - should be less 2048
              strcpy((char*)buf, http_404_full);
              rx_size = strlen((char*)buf);
              ret = send(sock_num, buf, rx_size);
              
              if(ret < 0)
              {
                close(sock_num);
                return ret;
              }
              
              //ending
              web_http_reset(sock_num);
              disconnect(sock_num);
            }//end of file size
            
          }
          // **************** POST ********************************************
          else if ((memcmp(buf, "POST ", 4) == 0) && (p = strchr(url, ' ')))
          {
            char* payload_start = strstr((const char*)buf, "\r\n\r\n");
            if (payload_start == NULL)
              return -1;
            else
              payload_start+= 4;
            
            char* cmd_start = strstr(payload_start, "command=");
            if (cmd_start == NULL)
              return -1;
            else
              cmd_start+= 8;
            
            if (strcmp(cmd_start, "reset_day_cnt") == 0)
            {
              power_reset_day_count();
            }
            else if (strcmp(cmd_start, "reset_month_cnt") == 0)
            {
              power_reset_month_count();
            }
            else if (memcmp(cmd_start, "total", 5) == 0)
            {
              //char* data_pos = cmd_start + 6;
              memset(str, 0, sizeof(str));
              decode_unicode(cmd_start, str);
              for (uint8_t i = 0; i < sizeof(str); i++)
              {
                if (str[i] == ',')
                  str[i] = '.';
              }
              
              char* data_start = strchr(str, '=');
              if (data_start == NULL)
                return -1;
              else
                data_start+= 1;
              
              float tmp_energy = atof(data_start);
              if (tmp_energy < 0.0f)
                return -1;
              power_set_total_count(tmp_energy);
            }
            
            web_http_reset(sock_num);
            disconnect(sock_num);
          }
        }
      }//end of getSn_RX_RSR
      
      if(http_state[sock_num] == HTTP_SENDING)
      {
        file_size = url_exists((char*)&http_url[sock_num][0]);
        //sending answer
        if(file_size != sentsize[sock_num])
        {
          if (header_sz > 0)
          {
            ret = send(sock_num, buf, header_sz);//отправка заголовка
          }
          else
          {
            bytes_read = f_read((char*)&http_url[sock_num][0], &buf[header_sz], WEB_DATA_BUF_SIZE, sentsize[sock_num]);
            ret = send(sock_num, buf, bytes_read);
          }
          
          if(ret < 0)
          {
            close(sock_num);
            return ret;
          }
          
          if (header_sz == 0) 
            sentsize[sock_num] += ret; // Don't care SOCKERR_BUSY, because it is zero.
        }
        
        if(sentsize[sock_num] >= file_size)
        {
          //ending
          web_http_reset(sock_num);
          disconnect(sock_num);
        }
      }//end of HTTP_SENDING

      break;
    
  case SOCK_CLOSE_WAIT :
    web_http_reset(sock_num);
#ifdef DEBUG
    printf("%d:CloseWait\r\n",sock_num);
#endif
    if((ret=disconnect(sock_num)) != SOCK_OK) 
      return ret;
#ifdef DEBUG
    printf("%d:Closed\r\n",sock_num);
#endif
    break;
    
  case SOCK_INIT :
    web_http_reset(sock_num);
#ifdef DEBUG
    printf("%d:Listen, port [%d]\r\n",sock_num, port);
#endif
    if( (ret = listen(sock_num)) != SOCK_OK) 
      return ret;
    break;
    
  case SOCK_CLOSED:
    web_http_reset(sock_num);
#ifdef DEBUG    
    printf("%d:LBTStart\r\n",sock_num);
#endif
    if((ret=socket(sock_num,Sn_MR_TCP,port,0x00)) != sock_num)
      return ret;
#ifdef DEBUG
    printf("%d:Opened\r\n",sock_num);
#endif
    break;
  default:
    {
      web_http_reset(sock_num);
      break;
    }
  }
  return 1;
}


//####################################
//проверяет наличие файла
uint32_t url_exists(char* file_name)
{
  if(strcmp(file_name, "index.html")==0)    
    return sizeof(index_file);
  if(strcmp(file_name, "zepto.min.js")==0)  
    return sizeof(zepto_min_js_gzip);
  if(strcmp(file_name, "state.cgi")==0)     
    return generate_json_data1(); //generated file
  return 0;
}


//####################################
//читает "файл" из flash
uint16_t f_read(
               char *fp, 		/* Pointer to the file object */
               uint8_t *buff,		/* Pointer to data buffer */
               uint16_t bytes_to_read,	/* Number of bytes to read */
               uint32_t offset		/* Pointer to number of bytes read */
                 )
{
  uint8_t* file_pointer;
  uint32_t cur_file_size = 0;
  uint32_t bytes_remain = 0;
  
  if(strcmp(fp, "index.html") == 0) 
  {
    file_pointer = (uint8_t*)index_file;
    cur_file_size = sizeof(index_file);
  }
  else if(strcmp(fp, "zepto.min.js") == 0) 
  {
    file_pointer = (uint8_t*)zepto_min_js_gzip;
    cur_file_size = sizeof(zepto_min_js_gzip);
  }
  else if(strcmp(fp, "state.cgi")==0) 
  {
    file_pointer = (uint8_t*)json_buffer1;//generated file
    cur_file_size = json_data1_size;
  }
  
  if (cur_file_size == 0) 
    return 0;
  if (offset > cur_file_size) 
    return 0;
  
  bytes_remain = cur_file_size - offset;//число доступных для чтения байт
  if (bytes_remain >= bytes_to_read)
  {
    memcpy(buff, &file_pointer[offset], bytes_to_read);
    return bytes_to_read;//прочитали столько байт, сколько просили
  }
  else
  {
    memcpy(buff, &file_pointer[offset], bytes_remain);
    return bytes_remain;//прочитали оставшиеся байты
  }
}


inline int ishex(int x)
{
  return  (x >= '0' && x <= '9')	||
          (x >= 'a' && x <= 'f')	||
          (x >= 'A' && x <= 'F');
}

//take unicode string and fill "dec" string
int decode_unicode(const char* s, char* dec)
{
  char *o;
  const char *end = s + strlen(s);
  int c;
  
  for (o = dec; s <= end; o++) 
  {
    c = *s++;
    if (c == '+') c = ' ';
    else if (c == '%' && (!ishex(*s++)	||
                          !ishex(*s++)	||
                            !sscanf(s - 2, "%2x", &c)))
      return -1;
    
    if (dec) *o = c;
  }
  
  return o - dec;
}