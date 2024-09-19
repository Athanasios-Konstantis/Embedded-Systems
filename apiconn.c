#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <jansson.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#define QUEUESIZE 1000
#define LOOP 20
#define REQUEST_SIZE 4
#define MAXFLOAT 0x1.fffffep+127f

struct lws_context *context;
pthread_mutex_t cntr_mut;
time_t now;
struct tm *t;
char time_str[40];
FILE *file[REQUEST_SIZE];
static int interrupted = 0;
static int interrupted_intentionally = 0;
static int times_pinged = 0;
static struct lws *client_wsi = NULL;

//max and min price this second
float price[4] = {0,0,0,0};
float maxp[4] = {0,0,0,0};
long long volume_sum[4] = {0,0,0,0};
float minp[4] = {MAXFLOAT,MAXFLOAT,MAXFLOAT,MAXFLOAT};
bool flag_init[4] = {false,false,false,false};
float start_price[4] = {0,0,0,0};
const char* symbols[] = {"AAPL","AMZN","MSFT","BTC"};
void save_string(json_t* data);
void *producer();
void *consumer();
void *counter();

static struct lws *connect_to_server(struct lws_context *context);
static struct lws_context *create_context(void);

typedef struct{
	json_t* buf[QUEUESIZE];
	long head, tail;
	int full,empty;
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
} queue;

typedef struct{
  float buff[REQUEST_SIZE][15];
  long tail[REQUEST_SIZE];
} queue2;

queue *fifo;
queue2 *volumes;
queue2 *mean_prices;
queue *queueInit(void);
queue2 *queue2Init(void);
void queueDelete(queue *q);
void queue2Delete(queue2 *q);
void queueAdd(queue *q, json_t* p);
void queue2Add(queue2 *q, int index, long long in);
void queueDel(queue *q, json_t** out);
static void sigint_handler(int sig);

static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len) {
    static int i = 0;
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("Connection established\n");
            // Send subscription message
            {
              for(int i = 0; i < 4; i++){
                char msg[64];
                sprintf(msg,"{\"type\":\"subscribe\",\"symbol\":\"%s\"}",symbols[i]);
                size_t len_msg = strlen(msg);
                unsigned char buf1[LWS_SEND_BUFFER_PRE_PADDING + len_msg + LWS_SEND_BUFFER_POST_PADDING];
                memcpy(&buf1[LWS_SEND_BUFFER_PRE_PADDING], msg, len_msg);
                int n = lws_write(wsi, &buf1[LWS_SEND_BUFFER_PRE_PADDING], len_msg, LWS_WRITE_TEXT);
                if (n < 0) {
                    printf("Failed to send subscription message for %s\n",symbols[i]);
                }
              }
            }
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("Received message: %s\n \t\t\t\t\t\t\t\t @ time %ld\n", (char *)in,time(NULL));
            //printf("LENGTH OF MESSAGE %d\n",(int)len);
            if(len > 0){
              json_error_t error;
              json_t *whole_msg = json_loads((char *)in, 0, &error);
              if (!whole_msg) {
                fprintf(stderr, "Error getting JSON from server: %s\n", error.text);
              }
              json_t *data_array = json_object_get(whole_msg, "data");
              if (!json_is_array(data_array)) {

                  json_t *type = json_object_get(whole_msg,"type");
                  if(!json_is_string(type)){
                    printf("type is not an array \n");
                    fprintf(stderr, "\"data\" is not an array or does not exist.\n");
                    json_decref(whole_msg);
                    return 0;
                  } 
                  if(strcmp("ping",json_string_value(type)) == 0){
                    if(++times_pinged == 3){
                      interrupted = 1;
                    }
                  printf("TIMES PINGED = %d\n",times_pinged);
                  return 0;
                  }
              }    
              times_pinged = 0;
              for(int i = 0; i < (int)json_array_size(data_array); i++) {
                  // Access the fields in each object
                  json_t *data = json_array_get(data_array,i);
                  if(!json_is_object(data)) // checking if the extracted element is a JSON object
                  {
                    fprintf(stderr, "Error: Data %d is not an object\n", i);
                    lws_context_destroy(context);
                    json_decref(data);//see this line again
                    exit(EXIT_FAILURE);
                  }//dont forget to json_decref them in the end i guess
                  if(!fifo->full){
                    queueAdd(fifo, data);    
                    printf("ADDED ITEM TO QUEUE\n");
                  }else{
                    printf("LOST DATA\n");
                    //exit(0);
                  }
              }
            }else{fprintf(stderr,"len less than or equal to 0\n");}
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf("Connection error\n");
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("Connection closed\n");
            interrupted = 1;
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
  {
    "websocket_protocol",
    ws_callback,
  },
  {NULL, NULL, 0, 0} // Terminator
};

int main() {

	pthread_t pro,con1,con2,con3,cntr;
  now = time(NULL);
  t = localtime(&now);
  strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S.json", t);
  for(int i = 0; i < REQUEST_SIZE; i++){
    char filename[20];
    sprintf(filename,"%s.txt",symbols[i]);
    file[i] = fopen(filename,"w");

    if(file[i] == NULL){
      printf("Error opening file\n");
      return -1;
    }
  }
  
  pthread_mutex_init(&cntr_mut,NULL);
  signal(SIGINT, sigint_handler);

	fifo = queueInit();
  volumes = queue2Init();
  mean_prices = queue2Init();

	if(fifo == NULL || volumes == NULL){
		fprintf(stderr, "main: Queue Init failed.\n");
		exit(1);
	}
  

  pthread_create(&pro, NULL, producer, NULL);
	pthread_create(&con1, NULL, consumer, NULL);
  pthread_create(&con2, NULL, consumer, NULL);
  pthread_create(&con3, NULL, consumer, NULL);
  pthread_create(&cntr, NULL, counter, NULL);
	pthread_join(pro,NULL);
	pthread_join(con1,NULL);
  pthread_join(con2,NULL);
  pthread_join(con3,NULL);
  pthread_join(cntr,NULL);
  return 0;
}

static void sigint_handler(int sig){
  interrupted_intentionally = 1;
  lws_context_destroy(context);
  queue2Delete(volumes);
  queue2Delete(mean_prices);
  queueDelete(fifo);
  pthread_mutex_destroy(&cntr_mut);

  printf("INSIDE SIGINT");
  char filename[64];
  for(int i = 0; i < REQUEST_SIZE; i++){
    if(file[i]!=NULL)
      if(fclose(file[i]) != 0)
        printf("Couldn't close file");
  }
  exit(0);
}

void *producer()
{
  context = create_context();
  if (!context) return (NULL);

   while (1) {
    if(interrupted_intentionally) break;
    if (!client_wsi || interrupted) {
        if (interrupted) {
            printf("Reconnecting...\n");
            sleep(5); // Wait before reconnecting
        }
        client_wsi = connect_to_server(context);
        interrupted = 0;
    }
    if(!interrupted_intentionally){
      pthread_mutex_lock (fifo->mut);
      while (fifo->full) {
        printf ("producer: queue FULL.\n");
        pthread_cond_wait (fifo->notFull, fifo->mut);
      }
      lws_service(context, 1000);
      pthread_mutex_unlock (fifo->mut);
      pthread_cond_signal (fifo->notEmpty);
    }

    if (interrupted){
        lws_context_destroy(context);
        context = create_context();
        if (!context) break;
    }
  }
  return (NULL);
}

void *consumer()
{
  int i;
  json_t* msg;

  while(1){
    pthread_mutex_lock (fifo->mut);
    while (fifo->empty) {
      //printf("consumer: queue EMPTY.\n");
      pthread_cond_wait (fifo->notEmpty, fifo->mut);
    }
    queueDel(fifo, &msg);
    printf("CONSUMER DELETED ITEM FROM QUEUE\n");
    
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);
    pthread_mutex_lock(&cntr_mut);
    save_string(msg);
    pthread_mutex_unlock(&cntr_mut);
  }
  return (NULL);
}
queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);

  for(int index = 0; index < QUEUESIZE; index++){
    q->buf[index] = NULL;
  }
	
  return (q);
}

queue2 *queue2Init(void){
  queue2 *q;

  q = (queue2 *)malloc( sizeof(queue2));
  if(q == NULL) return (NULL);
  for(int index = 0; index < 15; index++){
    for(int j = 0; j < REQUEST_SIZE; j++){
      q->buff[j][index] = 0;  
      q->tail[j] = 0;
    }
    
  }
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queue2Delete(queue2 *q){
  free(q);
}

void queueAdd (queue *q, json_t* p)
{
  q->buf[q->tail] = p;
  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queue2Add(queue2 *q,int index, long long in){
  q->buff[index][q->tail[index]] = in;
  q->tail[index]++;
  if(q->tail[index] == 15)
    q->tail[index] = 0;
}

void queueDel (queue *q, json_t** out)
{
  *out = q->buf[q->head];
  q->buf[q->head] = NULL;
  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}
long long queue2Get(queue2 *q, int index){
  long long sum = 0;
  for(int i = 0; i < 15; i++){
    sum += q->buff[index][i];
  }
  return sum;
}



void save_string(json_t* data){
    static int price_index[4] = {0,0,0,0};
    json_error_t error;
    json_t *c_array = json_object_get(data, "c");
    json_t *p_value = json_object_get(data, "p");
    json_t *s_value = json_object_get(data, "s");
    json_t *t_value = json_object_get(data, "t");
    json_t *v_value = json_object_get(data, "v");


    if (!c_array || !p_value || !s_value || !t_value || !v_value) {
      fprintf(stderr, "Error: One or more JSON values are NULL.\n");
      return;  // or handle the error appropriately
    }
    long long int t = json_integer_value(t_value);
    long long int v = json_integer_value(v_value);
    float p = json_real_value(p_value);
    for(int i = 0; i < REQUEST_SIZE; i++){
        if(strcmp(json_string_value(s_value),symbols[i]) == 0){
          fprintf(file[i],"p:%f\ts:%s\tt:%lld\tv:%lld\n",p,json_string_value(s_value),t,v);
          if(p > maxp[i]) maxp[i] = p;
          if(minp[i] > p) minp[i] = p;
          if(!flag_init[i]){
            flag_init[i] = true;
            start_price[i] = p;
          }
          price[i]=p;
          volume_sum[i] += v;
        }
    }
}  


void *counter(){
  //open files and stuff
  while(1){
    printf("Sleeping for 1 second...\n");
    usleep(60000000);  // Sleep for 1 min
    printf("Awoke from sleep.\n");
    pthread_mutex_lock(&cntr_mut);
    //open files n shit
    for(int i = 0; i < REQUEST_SIZE; i++){
      if(maxp[i]!= 0 && MAXFLOAT - minp[i] > 0.1){
        printf("s=%s\tmax = %f\tmin = %f\tv=%lld\tinit_price=%f\t final_price = %f\n",symbols[i],maxp[i],minp[i],volume_sum[i],start_price[i],price[i]);
        queue2Add(volumes,i,volume_sum[i]);
        printf("volume in 15 mins: %lld\n",queue2Get(volumes,i));
      }else{
        printf("no data for %s this min\n",symbols[i]);
      }
      maxp[i] = 0;
      minp[i] = MAXFLOAT;
      flag_init[i] = false;
      start_price[i] = 0;
      volume_sum[i] = 0;
    }
    
    //reset max and min float values for the next second


    pthread_mutex_unlock(&cntr_mut);
  }
}

static struct lws_context *create_context(void){
  struct lws_context_creation_info info;
  memset(&info, 0, sizeof(info));
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.iface = NULL;

  struct lws_context *context = lws_create_context(&info);
  if (!context) {
      fprintf(stderr, "Error creating context\n");
  }
  return context;
}

static struct lws *connect_to_server(struct lws_context *context){
  struct lws_client_connect_info client;
  memset(&client, 0, sizeof(client));
  client.context = context;
  client.address = "ws.finnhub.io";
  client.port = 443;
  client.path = "/?token=cqlndu9r01qo3h6tkppgcqlndu9r01qo3h6tkpq0";
  client.host = client.address;
  client.origin = client.address;
  client.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
  client.protocol = protocols[0].name;

  return lws_client_connect_via_info(&client);


}