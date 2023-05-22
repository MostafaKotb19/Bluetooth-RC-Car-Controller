/*
 The code is written for an Arduino Uno with an HC-05 Bluetooth module and L298 Motor Driver connected to it. Here are some key points:
 * The code includes necessary libraries and defines constants for the baud rate and pin assignments.
 * The UART_init function initializes the UART communication with the specified baud rate.
 * There are functions to send and receive characters over UART.
 * The setSpeed function adjusts the motor speed using PWM (Pulse Width Modulation).
 * Functions like forward, backward, left, right, and stopp control the motor movements by setting the corresponding pin values.
 * The code sets up an interrupt service routine (ISR) to handle received UART data.
 * The timer_init function configures timers for PWM operation.
 * In the main function, the motor and speed control pins are set as outputs, and the UART and timers are initialized.
 * The code enters an infinite loop and checks for received data. When data is received, it is processed and the appropriate motor action is performed.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>

#define BAUD_RATE 38400
#define M1_SPEED_PIN PD6
#define M1_FORWARD_PIN PD7
#define M1_BACKWARD_PIN PB0
#define M2_FORWARD_PIN PB2
#define M2_BACKWARD_PIN PB1
#define M2_SPEED_PIN PB3

volatile char c = ' ';
volatile int speed = 255;
volatile bool dataReceived = false;

// UART initialization
void UART_init(uint16_t baud_rate)
{
  // Calculate the UART baud rate register value
  uint16_t ubrr = F_CPU / 16 / baud_rate - 1;
  
  // Set the baud rate registers
  UBRR0H = (uint8_t)(ubrr >> 8);
  UBRR0L = (uint8_t)ubrr;
  
  // Enable UART receiver and transmitter, enable RX complete interrupt
  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
  
  // Set frame format: 8 data bits, 1 stop bit
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Send a single character over UART
void UART_sendChar(char data)
{
  // Wait for the UART data register to be ready for new data
  while (!(UCSR0A & (1 << UDRE0)))
    ;
  
  // Write the character to the UART data register
  UDR0 = data;
}

// Send a null-terminated string over UART
void UART_sendString(const char* str)
{
  // Iterate through the string characters and send them one by one
  for (size_t i = 0; i < strlen(str); i++)
  {
    UART_sendChar(str[i]);
  }
}

// Receive a single character over UART
char UART_receiveChar()
{
  // Wait for data to be received
  while (!(UCSR0A & (1 << RXC0)))
    ;
  
  // Read and return the received character from the UART data register
  return UDR0;
}

// Check if data is available to be received over UART
bool UART_dataAvailable()
{
  // Check the receive complete flag in the UART status register
  return UCSR0A & (1 << RXC0);
}

// Check if a character is a digit
bool isDigit(char c)
{
  return (c >= '0' && c <= '9');
}

// Set the motor speed
void setSpeed(char s)
{
  speed = s - '0';
  OCR0A = (speed * 255) / 10;
  OCR2A = (speed * 255) / 10;
  UART_sendString("Speed set to ");
  if (s == ':')
    UART_sendString("10");
  else
    UART_sendChar(s);
  UART_sendString("\r\n");
}

// Move the motors forward
void forward()
{
  PORTD |= (1 << M1_FORWARD_PIN);
  PORTB &= ~(1 << M1_BACKWARD_PIN);
  PORTB |= (1 << M2_FORWARD_PIN);
  PORTB &= ~(1 << M2_BACKWARD_PIN);
  UART_sendString("forward\r\n");
}

// Move the motors backward
void backward()
{
  PORTD &= ~(1 << M1_FORWARD_PIN);
  PORTB |= (1 << M1_BACKWARD_PIN);
  PORTB &= ~(1 << M2_FORWARD_PIN);
  PORTB |= (1 << M2_BACKWARD_PIN);
  UART_sendString("backward\r\n");
}

// Turn left
void left()
{
  PORTD &= ~(1 << M1_FORWARD_PIN);
  PORTB |= (1 << M1_BACKWARD_PIN);
  PORTB |= (1 << M2_FORWARD_PIN);
  PORTB &= ~(1 << M2_BACKWARD_PIN);
  UART_sendString("left\r\n");
}

// Turn right
void right()
{
  PORTD |= (1 << M1_FORWARD_PIN);
  PORTB &= ~(1 << M1_BACKWARD_PIN);
  PORTB &= ~(1 << M2_FORWARD_PIN);
  PORTB |= (1 << M2_BACKWARD_PIN);
  UART_sendString("right\r\n");
}

// Stop the motors
void stopp()
{
  PORTD &= ~(1 << M1_FORWARD_PIN);
  PORTB &= ~(1 << M1_BACKWARD_PIN);
  PORTB &= ~(1 << M2_FORWARD_PIN);
  PORTB &= ~(1 << M2_BACKWARD_PIN);
  //UART_sendString("stop\r\n");
}

// UART receive interrupt service routine
ISR(USART_RX_vect)
{
  c = UDR0;
  dataReceived = true;
}

// Initialize timers for PWM
void timer_init()
{
  TCCR0A = (1 << WGM00) | (1 << WGM01) | (1 << COM0A1) | (1 << COM0B1);
  TCCR0B = (1 << CS00);
  TCCR2A = (1 << COM2A1) | (1 << WGM21) | (1 << WGM20);
  TCCR2B = (1 << CS20);
  OCR2A = 255;
  OCR0A = 255;
}

int main(void)
{
  // Set motor pins as output
  DDRD |= (1 << M1_FORWARD_PIN);
  DDRB |= (1 << M1_BACKWARD_PIN) | (1 << M2_FORWARD_PIN) | (1 << M2_BACKWARD_PIN);
  
  // Set speed control pins as output
  DDRD |= (1 << M1_SPEED_PIN);
  DDRB |= (1 << M2_SPEED_PIN);
  
  // Initialize UART and timers
  UART_init(BAUD_RATE);
  timer_init();
  
  // Enable global interrupts
  sei();

  while (1)
  {
    // Check if data has been received
    if (dataReceived)
    {
      // Echo the received character back over UART
      if (c != 'S')
        UART_sendChar(c);
      dataReceived = false;

      // Check if the received character is a digit
      if (isDigit(c))
      {
        // Convert the character to an integer speed value
        setSpeed(c);
      }
      else
      {
        // Based on the received character, perform the corresponding action
        if (c == 'q')
        {
          setSpeed(':'); //Speed 10
        }
        else if (c == 'F')
        {
          forward();
        }
        else if (c == 'B')
        {
          backward();
        }
        else if (c == 'L')
        {
          left();
        }
        else if (c == 'R')
        {
          right();
        }
        else if (c == 'S')
        {
          stopp();
        }
        else
        {
          stopp();
        }
      }
    }
  }
}
