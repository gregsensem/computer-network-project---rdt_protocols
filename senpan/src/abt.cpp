#include "../include/simulator.h"
#include <queue>
#include <string>
#include <string.h>
#include <vector>
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
    int seq_num;
    int pkt_seq_num;
    int state; // set the state to 0 when sender is waiting for msg, set to 1 when waiting for ACK;
    float pkt_sent_time;
    float timeout_interval;
    struct pkt last_sent_pkt;
    std::queue<struct pkt> pkt_queue;
    Sender() : seq_num(0), pkt_seq_num(0), state(0), pkt_sent_time(0), timeout_interval(20) {};
};

Sender *A;

class Reciver
{
  public:
    int expected_seq;
    Reciver() : expected_seq(0) {};
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

void send_paket(const struct pkt &p)
{
    //send pkt to layer 3
    tolayer3(0, p);
    //start timer
    A->pkt_sent_time = get_sim_time();
    starttimer(0, A->timeout_interval);
    //change the sate of A to waiting for message from layer 5
    A->state = 1;
    //update the last sent pkt;
    A->last_sent_pkt = p;
}

void make_ack_packet(const struct pkt &packet, struct pkt &ack_pkt)
{
  ack_pkt.seqnum = 0;
  ack_pkt.acknum = packet.seqnum;
  memset(&ack_pkt.payload[0], 0, sizeof(ack_pkt.payload));
  ack_pkt.checksum = checksum(ack_pkt); 
}

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
  //prepare the packet
  struct pkt p;
  make_paket(message, p, A->pkt_seq_num, 0);
  A->pkt_seq_num = (A->pkt_seq_num == 0 ? 1 : 0);

  //Add packet to the queue
  A->pkt_queue.push(p);

  //check the state of the sender
  //if the state of the sender is waiting, send the message
  printf("A is in State %d!\n", A->state);

  if(A->state == 0)
  {
    struct pkt pkt_to_send = A->pkt_queue.front();
    A->pkt_queue.pop();

    //send packet
    send_paket(pkt_to_send);
    A->seq_num = pkt_to_send.seqnum;
    printf("Succesfully sent SEQ%d from A\n", pkt_to_send.seqnum);
  }
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
  //check if the ACK number is as expected, ignore if it is duplicate ACK;
  if(packet.acknum != A->seq_num)
  {
    printf("ACK num not matched in A side!");
    return;
  }
  //check if the packet is corrupted
  if(!pass_checksum(packet))
  {
    printf("Checksum error in A side!");
    return;
  }

  //check if A is in state of waiting for ACK
  if(A->state == 0)
  {
    printf("State error in A side!");
    return;
  }

  //Successfully received the ACK for last sent pkt, 
  //Stop the timer and change the state to wait for msg and send message in the buffer.
  printf("Succesfully received ACK %d from B\n", packet.acknum);

  stoptimer(0);
  A->state = 0;

  //check if there are still messges in the buffer to be sent
  if(!A->pkt_queue.empty())
  {
    struct pkt pkt_to_send = A->pkt_queue.front();
    A->pkt_queue.pop();
    send_paket(pkt_to_send);
    A->seq_num = pkt_to_send.seqnum;
    printf("Sent PKT %d\n", pkt_to_send.seqnum);
  }
  else
  {
    printf("Buffer is empty\n");
  }
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
    //Resend last packet
    send_paket(A->last_sent_pkt);
    printf("Sent PKT %d\n", A->last_sent_pkt.seqnum);
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  A = new Sender();
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
  //check if the packet is corrupted
  if(!pass_checksum(packet))
  {
    printf("Checksum error in B side!");
    return;  
  }

  //send the packet to layer 5 if it is the expected packet
  if(packet.seqnum == B->expected_seq)
  {
    tolayer5(1, packet.payload);
    //update B's next expected sequence number
    B->expected_seq = (B->expected_seq == 0 ? 1 : 0);
  }
  else
  {
    printf("Seq number %d not matched with expected Seq number %d!", packet.seqnum, B->expected_seq);
  }
  

  //prepare ACK packet and reply to A
  struct pkt ack_pkt;
  make_ack_packet(packet, ack_pkt);
  tolayer3(1, ack_pkt);
  printf("Sent ACK %d", ack_pkt.acknum);
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  B = new Reciver();
}
