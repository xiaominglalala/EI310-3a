//������������ͷ�ļ�tm1638.h����project��
//����������DAC6571�������涨��P1.4��SDA(pin4),P1.5��SCL(pin5)
//����������DAC6571���֣�����ȫ�ֱ������ڳ̿�DACоƬ�Ĳ���
//��dac6571���ƿ�ͷ�����κ����ǳ̿�DACоƬ�ĺ��Ĵ���
//�������� 2�� +100��3�� +10��4�� +1��6�� -100��7�� -10��8�� -1��

//������ʱ�Ӳ����ڲ�RC������     DCO��8MHz,��CPUʱ��;  SMCLK��1MHz,����ʱ��ʱ��
#include <msp430g2553.h>
#include <tm1638.h>  //��TM1638�йصı���������������ڸ�H�ļ���

//////////////////////////////
//         ��������         //
//////////////////////////////

// 0.5s�����ʱ�����ֵ��25��20ms
#define V_T500ms	25

//DAC6571����  P1.4��SDA(pin4),P1.5��SCL(pin5)
#define SCL_L       P1OUT&=~BIT5
#define SCL_H       P1OUT|=BIT5
#define SDA_L       P1OUT&=~BIT4
#define SDA_H       P1OUT|=BIT4
#define SDA_IN      P1OUT|=BIT4; P1DIR&=~BIT4; P1REN|=BIT4
#define SDA_OUT     P1DIR|=BIT4; P1REN&=~BIT4
#define DAC6571_code_max      1023  // 10λADC
#define DAC6571_address         0x98  // 1001 10 A0 0  A0=0


//////////////////////////////
//       ��������           //
//////////////////////////////

// �����ʱ������
unsigned char clock500ms=0;
// �����ʱ�������־
unsigned char clock500ms_flag=0;
// �����ü�����
unsigned char digit[8]={' ',' ',' ',' ',' ',' ',' ',' '};
// 8λС���� 1��  0��
// ע����������λС����������������Ϊ4��5��6��7��0��1��2��3
unsigned char pnt=0x00;
// 8��LEDָʾ��״̬��ÿ����4����ɫ״̬��0��1�̣�2�죬3�ȣ���+�̣�
// ע������ָʾ�ƴ������������Ϊ7��6��5��4��3��2��1��0
//     ��ӦԪ��LED8��LED7��LED6��LED5��LED4��LED3��LED2��LED1
unsigned char led[]={0,0,1,1,2,2,3,3};
// ��ǰ����ֵ
unsigned char key_code=0;
unsigned char key_cnt=0;

// DAC6571
int dac6571_code=(DAC6571_code_max>>2);
unsigned char dac6571_flag=0;

//////////////////////////////
//       ϵͳ��ʼ��         //
//////////////////////////////

//  I/O�˿ں����ų�ʼ��
void Init_Ports(void)
{
	P2SEL &= ~(BIT7+BIT6);       //P2.6��P2.7 ����Ϊͨ��I/O�˿�
	  //������Ĭ�������⾧�񣬹�����޸�

	P2DIR |= BIT7 + BIT6 + BIT5; //P2.5��P2.6��P2.7 ����Ϊ���
	  //����·������������������ʾ�ͼ��̹�����TM1638������ԭ�������DATASHEET

	P1DIR |= BIT4 + BIT5;
	P1OUT |= BIT4 + BIT5;
 }

//  ��ʱ��TIMER0��ʼ����ѭ����ʱ20ms
void Init_Timer0(void)
{
	TA0CTL = TASSEL_2 + MC_1 ;      // Source: SMCLK=1MHz, UP mode,
	TA0CCR0 = 20000;                // 1MHzʱ��,����20000��Ϊ 20ms
	TA0CCTL0 = CCIE;                // TA0CCR0 interrupt enabled
}

//  MCU������ʼ����ע���������������
void Init_Devices(void)
{
	WDTCTL = WDTPW + WDTHOLD;     // Stop watchdog timer��ͣ�ÿ��Ź�
	if (CALBC1_8MHZ ==0xFF || CALDCO_8MHZ == 0xFF)
	{
		while(1);            // If calibration constants erased, trap CPU!!
	}

    //����ʱ�ӣ��ڲ�RC������     DCO��8MHz,��CPUʱ��;  SMCLK��1MHz,����ʱ��ʱ��
	BCSCTL1 = CALBC1_8MHZ; 	 // Set range
	DCOCTL = CALDCO_8MHZ;    // Set DCO step + modulation
	BCSCTL3 |= LFXT1S_2;     // LFXT1 = VLO
	IFG1 &= ~OFIFG;          // Clear OSCFault flag
	BCSCTL2 |= DIVS_3;       //  SMCLK = DCO/8

    Init_Ports();           //���ú�������ʼ��I/O��
    Init_Timer0();          //���ú�������ʼ����ʱ��0
    _BIS_SR(GIE);           //��ȫ���ж�
   //all peripherals are now initialized
}

void dac6571_byte_transmission(unsigned char byte_data)
{
	unsigned char i,shelter;
	shelter = 0x80;

	for (i=1; i<=8; i++)
	{
		if ((byte_data & shelter)==0) SDA_L;
		else SDA_H;
		SCL_H; SCL_L;
		shelter >>= 1;
	}
	SDA_IN;
	SCL_H; SCL_L;
	SDA_OUT;
}

void dac6571_fastmode_operation(void)
{
	unsigned char msbyte,lsbyte;

	SCL_H; SDA_H; SDA_L; SCL_L;       // START condition
	dac6571_byte_transmission(DAC6571_address);
	msbyte = dac6571_code*4/256;
	lsbyte = dac6571_code*4 - msbyte * 256;
	dac6571_byte_transmission(msbyte);
	dac6571_byte_transmission(lsbyte);

    SDA_L; SCL_H; SDA_H;        // STOP condition
}

//////////////////////////////
//      �жϷ������        //
//////////////////////////////

// Timer0_A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void)
{
 	// 0.5������ʱ������
	if (++clock500ms>=V_T500ms)
	{
		clock500ms_flag = 1; //��0.5�뵽ʱ�������־��1
		clock500ms = 0;
	}

	// ˢ��ȫ������ܺ�LEDָʾ��
	TM1638_RefreshDIGIandLED(digit,pnt,led);

	// ��鵱ǰ�������룬0�����޼�������1-16��ʾ�ж�Ӧ����
	//   ������ʾ����λ�������
	key_code=TM1638_Readkeyboard();
	if (key_code != 0)
	{
		if (key_cnt<3) key_cnt++;
		else if (key_cnt==3)
		{
			switch (key_code)
			{
			case 2: dac6571_code+=100;dac6571_flag=1;break; //2����100
			case 6: dac6571_code-=100;dac6571_flag=1;break; //6����100
			case 3: dac6571_code+= 10;dac6571_flag=1;break; //3����10
			case 7: dac6571_code-= 10;dac6571_flag=1;break; //7����10
			case 4: dac6571_code++;   dac6571_flag=1;break; //4����1
			case 8: dac6571_code--;   dac6571_flag=1;break; //8����1
			default:break;
			}
			if (dac6571_code>DAC6571_code_max) dac6571_code=DAC6571_code_max;
			else if (dac6571_code<0) dac6571_code=0;

			key_cnt =4;
		}
	}
	else key_cnt=0;

}

//////////////////////////////
//         ������           //
//////////////////////////////

int main(void)
{
	unsigned char i=0;
	float temp;
	Init_Devices( );
	while (clock500ms<1);   // ��ʱ�㹻ʱ��ȴ�TM1638�ϵ����
	init_TM1638();	    //��ʼ��TM1638
	dac6571_flag = 1;

	while(1)
	{
		if (dac6571_flag==1)   // ���DAC��ѹ�Ƿ�Ҫ��
		{
			dac6571_flag=0;

			digit[0] = dac6571_code/1000%10; 	//����ǧλ 0-1023
			digit[1] = dac6571_code/100%10; 	//�����λ
			digit[2] = dac6571_code/10%10;      //����ʮλ
			digit[3] = dac6571_code%10;      //�����λ

			dac6571_fastmode_operation();
		}


		if (clock500ms_flag==1)   // ���0.5�붨ʱ�Ƿ�
		{
			clock500ms_flag=0;
			// 8��ָʾ��������Ʒ�ʽ��ÿ0.5�����ң�ѭ�����ƶ�һ��
			temp=led[0];
			for (i=0;i<7;i++) led[i]=led[i+1];
			led[7]=temp;
		}
	}
}
