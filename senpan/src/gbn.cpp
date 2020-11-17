#include "../include/simulator.h"
#include <queue>
#include <string>
#include <string.h>
#include <vector>
#include <list>
#include <iostream>
#include <stdint.h>

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
    std::list<struct pkt> resend_queue;
    Sender(int _wind_size) : base_num(0), next_seqnum(0), pkt_seqnum(0), max_seqnum(2*_wind_size), wind_size(_wind_size), pkt_sent_time(0), timeout_interval(20) {};
};

Sender *A;

class Reciver
{
  public:
    int expected_seq;
    int last_acked;
    int max_seqnum;
    Reciver(int _wind_size) : expected_seq(0), last_acked(-1), max_seqnum(2*_wind_size) {};
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
    //push it to the resend queue
    A->resend_queue.push_back(pkt_to_send);

    //send packet
    tolayer3(0, pkt_to_send);

    //start timer if the pkt is the base pkt
    if(A->base_num == A->next_seqnum)
      starttimer(0, A->timeout_interval);

    //Update next seqnum
    A->next_seqnum = (pkt_to_send.seqnum + 1) % A->max_seqnum;

    printf("Sent PKT%d from A\n", pkt_to_send.seqnum);
  }
  else
  {
    printf("Sending window is full!\n");
  }
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
  //check if the packet is corrupted
  if(!pass_checksum(packet))
  {
    printf("Checksum error in A side!");
    return;
  }

  if(packet.acknum < A->base_num || packet.acknum >= (A->base_num + A->wind_size))
  {
    printf("The ack num not in the window size\n");
    return;
  }
  //pop the successfully acked pkts from the resend queue
  int pkt_num;
  do
  {
    pkt_num = A->resend_queue.front().seqnum;
    A->resend_queue.pop_front();
  }while(pkt_num != packet.acknum);

  //Advance base num according to accumulative ack
  A->base_num = (packet.acknum + 1) % A->max_seqnum;

  if(A->base_num == A->next_seqnum)
    stoptimer(0);
  else
    starttimer(0,A->timeout_interval);
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  starttimer(0, A->timeout_interval);
  
  //resend all the pkts in the resend queue
  for(auto const& it : A->resend_queue)
    tolayer3(0, it);
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
    printf("Checksum error in A side!");
    return;
  }
  
  //check if the Seq number is as expected, ignore if it is not;
    //send the packet to layer 5 if it is the expected packet
  if(packet.seqnum == B->expected_seq)
  {
    tolayer5(1, packet.payload);
    //update B's next expected sequence number
    B->expected_seq = (B->expected_seq + 1) % B->max_seqnum;
    //update B's last ACKed num
    B->last_acked = packet.seqnum;

    //prepare ACK packet and reply to A
    struct pkt ack_pkt;
    make_ack_packet(packet.seqnum, ack_pkt);
    tolayer3(1, ack_pkt);
    printf("Sent ACK %d", ack_pkt.acknum);
  }
  else
  {
    printf("Not expected sequence number!");
    if(B->last_acked == -1)
      return;
    else
    {
      //Send duplicate accumulative ack
      struct pkt ack_pkt;
      make_ack_packet(B->last_acked, ack_pkt);
      tolayer3(1, ack_pkt);
      printf("Sent ACK %d", ack_pkt.acknum);
    }
  } 
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  int wind_size = getwinsize();
  B = new Reciver(wind_size);
}
