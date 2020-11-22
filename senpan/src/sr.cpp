#include "../include/simulator.h"
#include <queue>
#include <string>
#include <string.h>
#include <vector>
#include <map>
#include <list>
#include <iostream>
#include <stdint.h>
#include <algorithm>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/
class virtual_timer
{
public:
    float time;
    int seqnum;
    virtual_timer(float _time,int _seqnum) : 
        time(_time), seqnum(_seqnum) {};
    bool operator < (const virtual_timer &time2) const
    {
        return time < time2.time;
    }
};

class Sender
{
  public:
    int base_num;
    int next_seqnum;
    int pkt_seqnum;
    int max_seqnum;
    int wind_size;
    float pkt_sent_time;
    float timeout_interval;
    std::queue<struct pkt> pkt_queue;
    std::vector<struct pkt> resend_buffer;
    std::vector<virtual_timer> virtual_timer_list;
    Sender(int _wind_size) : base_num(0), next_seqnum(0), pkt_seqnum(0), max_seqnum(2*_wind_size), wind_size(_wind_size), pkt_sent_time(0), timeout_interval(20) {};
};

Sender *A;

class Reciver
{
  public:
    int recv_base_num;
    int max_seqnum;
    int wind_size;
    std::map<int, struct pkt> recv_buffer;

    Reciver(int _wind_size) : recv_base_num(0), max_seqnum(2*_wind_size), wind_size(_wind_size) {};
};

Reciver *B;

int checksum(const struct pkt &p)
{
    uint8_t sum = 0;
    sum += static_cast<uint8_t>(p.seqnum);
    sum += static_cast<uint8_t>(p.acknum);
    int payload_size = 20;
    for(int i = 0; i < payload_size; i++)
    {
      sum += static_cast<uint8_t>(p.payload[i]);
    }
    //cast sum back to int
    int sum_int = static_cast<int>(sum);

    return sum_int;
}

bool pass_checksum(const struct pkt &p)
{
  return p.checksum == checksum(p);
}

void print_timer()
{
  if(!A->virtual_timer_list.empty())
  {
    for(auto it : A->virtual_timer_list)
    {
      printf("DEBUG: Interrupt time is %f for PKT %d\n", it.time, it.seqnum);
    }
  }
  else
  {
    printf("DEBUG: Virtual timer list is empty!\n");
  }
}

void print_recv_buffer()
{
  if(!B->recv_buffer.empty())
  {
    for (int i = 0; i < B->max_seqnum; i++)
    if(B->recv_buffer.find(i) != B->recv_buffer.end())
      printf("DEBUG: PKT %d  %s is in the recv buffer!\n", i, B->recv_buffer.at(i).payload);
  }
  else
  {
    printf("DEBUG:Recv buffer is empty!\n");
  }
}

void make_paket(const struct msg &msg, struct pkt &p, int seq_num, int ack_num)
{
  p.seqnum = seq_num;
  p.acknum = ack_num;
  strcpy(p.payload, msg.data);
  p.checksum = checksum(p);
}

void make_ack_packet(const int acknum, struct pkt &ack_pkt)
{
  ack_pkt.seqnum = 0;
  ack_pkt.acknum = acknum;
  memset(&ack_pkt.payload[0], 0, sizeof(ack_pkt.payload));
  ack_pkt.checksum = checksum(ack_pkt); 
}

void send_paket(const struct pkt& p)
{
  //send pkt to layer 3
  tolayer3(0, p);

  //if p is the base, start timer
  if(A->base_num == A->next_seqnum)
  {
    starttimer(0, A->timeout_interval);
  }

  //add virtual timer to timer list
  A->virtual_timer_list.push_back(virtual_timer(get_sim_time() + A->timeout_interval, p.seqnum));

  //add pkt to resend buffer
  A->resend_buffer.push_back(p);

  //update the next seqnum
  A->next_seqnum = (p.seqnum + 1) % A->max_seqnum;

  printf("DEBUG: Sent PKT%d from A\n", p.seqnum);
  print_timer();
}

void resend_packet(const int pkt_num)
{
  for(auto it : A->resend_buffer)
  {
    if(it.seqnum == pkt_num)
    {
      tolayer3(0, it);
      
      //update timer list for pkt_num
      A->virtual_timer_list.erase(A->virtual_timer_list.begin());
      A->virtual_timer_list.push_back(virtual_timer(get_sim_time() + A->timeout_interval, it.seqnum));

      //restart timer for the first timer in the timer list
      printf("DEBUG: VIRTUAL TIMER START AT at: %f\n",A->virtual_timer_list[0].time - get_sim_time());
      starttimer(0, A->virtual_timer_list[0].time - get_sim_time());
      printf("DEBUG: Resent PKT%d from A\n",pkt_num);
      print_timer();
      return;
    }
  }
  //else the packet not found in resend buffer
  printf("DEBUG: time out pkt not found in resend buffer!\n");
}

void ack_paket(int ack_num)
{
  //remove the paket from the buffer list
  for(int i = 0; i < A->resend_buffer.size(); i++)
  {
    if(A->resend_buffer[i].seqnum == ack_num)
    {
      A->resend_buffer.erase(A->resend_buffer.begin() + i);
    }
  }

  //remove its timer from the timer list
  for(int i = 0; i < A->virtual_timer_list.size(); i++)
  {
    if(A->virtual_timer_list[i].seqnum == ack_num)
    {
      A->virtual_timer_list.erase(A->virtual_timer_list.begin() + i);
    }
  }

  if(A->resend_buffer.empty())
  {
    stoptimer(0);
    A->base_num = A->next_seqnum;
  }
  //update the timer and advance the base if pkt_num is the base
  else if(A->base_num == ack_num)
  {
    //resart the timer
    stoptimer(0);
    
    printf("DEBUG: VIRTUAL TIMER START AT at: %f\n",A->virtual_timer_list[0].time - get_sim_time());
    starttimer(0, A->virtual_timer_list[0].time - get_sim_time());

    A->base_num = A->resend_buffer[0].seqnum;
  }

  print_timer();
}


/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
  //prepare the packet
  struct pkt p;
  make_paket(message, p, A->pkt_seqnum, 0);
  A->pkt_seqnum = (A->pkt_seqnum + 1) % A->max_seqnum;

  //Add packet to the queue
  A->pkt_queue.push(p);

  //if next seq num is within the range of the window
  if((A->next_seqnum - A->base_num + A->max_seqnum) % A->max_seqnum < A->wind_size)
  {
    struct pkt pkt_to_send = A->pkt_queue.front();
    A->pkt_queue.pop();
    send_paket(pkt_to_send);
  }
  else
  {
    printf("DEBUG: Sending window is full!\n");
  }
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
 //check if the packet is corrupted
  if(!pass_checksum(packet))
  {
    printf("DEBUG: Checksum error in A side!\n");
    return;
  }

  if(packet.acknum < A->base_num || packet.acknum >= (A->base_num + A->wind_size))
  {
    printf("DEBUG: The ack num not in the window size\n");
    return;
  }

  ack_paket(packet.acknum);

  //check if there are new packets fall into the range of the window.
  //send these packets if yes
  while((A->next_seqnum != (A->base_num + A->wind_size)%A->max_seqnum) && !A->pkt_queue.empty())
  {
    struct pkt pkt_to_send = A->pkt_queue.front();
    A->pkt_queue.pop();
    send_paket(pkt_to_send);
  }
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  //resend the timeout packet
  int timeout_pkt = A->virtual_timer_list[0].seqnum;
  resend_packet(timeout_pkt);
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  int wind_size = getwinsize();
  A = new Sender(wind_size);
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
  //check if the packet is corrupted
  if(!pass_checksum(packet))
  {
    printf("DEBUG: Checksum error in B side!\n");
    return;
  }

  //if the packt seq num fall within the recv window
  printf("DEBUG: Calculation result is: %d\n", ((packet.seqnum - B->recv_base_num + B->max_seqnum) % B->max_seqnum) < B->wind_size);
  if(((packet.seqnum - B->recv_base_num + B->max_seqnum) % B->max_seqnum) < B->wind_size)
  {
    printf("DEBUG: Entered if\n");
    //check if the paket has already been received
    if(B->recv_buffer.find(packet.seqnum) == B->recv_buffer.end())
    {
      B->recv_buffer.insert(std::pair<int, struct pkt>(packet.seqnum, packet));
      // std::sort(B->recv_buffer.begin(), B->recv_buffer.end());
      printf("DEBUG: Added PKT%d into receiver buffer\n", packet.seqnum);
      if(packet.seqnum == B->recv_base_num)
      {
        printf("DEBUG: To Layer5 1\n");
        tolayer5(1, packet.payload);
        B->recv_buffer.erase(B->recv_base_num);
        //ACK packet
        struct pkt ack_pkt;
        make_ack_packet(packet.seqnum, ack_pkt);
        tolayer3(1, ack_pkt);

        //increment base num
        B->recv_base_num = (B->recv_base_num + 1) % B->max_seqnum;
        while(B->recv_buffer.find(B->recv_base_num) != B->recv_buffer.end())
        {
          printf("DEBUG: To Layer5 2\n");
          tolayer5(1, B->recv_buffer.at(B->recv_base_num).payload);
          B->recv_buffer.erase(B->recv_base_num);

          //increment base num
          B->recv_base_num = (B->recv_base_num + 1) % B->max_seqnum;          
        }
      }
      else
      {
        //out of order packet in the window range
        //ACK packet
        struct pkt ack_pkt;
        make_ack_packet(packet.seqnum, ack_pkt);
        tolayer3(1, ack_pkt);
      }
      
      print_recv_buffer();
    }
    else
    {
      //The packet has been received, send duplicate ack
      //Packet seq num is out of window range, Send duplicate ack
      struct pkt ack_pkt;
      make_ack_packet(packet.seqnum, ack_pkt);
      tolayer3(1, ack_pkt);
      printf("DEBUG: Sent ACK %d for duplicate packet in window range!\n", ack_pkt.acknum);
      
      print_recv_buffer();
    }
  }
  else
  {
    //Packet seq num is out of window range, Send duplicate ack
    struct pkt ack_pkt;
    make_ack_packet(packet.seqnum, ack_pkt);
    tolayer3(1, ack_pkt);
    printf("DEBUG: Sent ACK %d for duplicate packet out of window range!\n", ack_pkt.acknum);

    print_recv_buffer();
  }
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  int wind_size = getwinsize();
  B = new Reciver(wind_size);
}
