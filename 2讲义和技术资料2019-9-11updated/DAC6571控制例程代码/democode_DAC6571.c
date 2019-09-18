//本程序必须搭配头文件tm1638.h做成project包
//常量定义中DAC6571操作，规定了P1.4接SDA(pin4),P1.5接SCL(pin5)
//变量定义中DAC6571部分，定义全局变量用于程控DAC芯片的操作
//以dac6571名称开头的两段函数是程控DAC芯片的核心代码
//操作按键 2号 +100；3号 +10；4号 +1；6号 -100；7号 -10；8号 -1；

//本程序时钟采用内部RC振荡器。     DCO：8MHz,供CPU时钟;  SMCLK：1MHz,供定时器时钟
#include <msp430g2553.h>
#include <tm1638.h>  //与TM1638有关的变量及函数定义均在该H文件中

//////////////////////////////
//         常量定义         //
//////////////////////////////

// 0.5s软件定时器溢出值，25个20ms
#define V_T500ms	25

//DAC6571操作  P1.4接SDA(pin4),P1.5接SCL(pin5)
#define SCL_L       P1OUT&=~BIT5
#define SCL_H       P1OUT|=BIT5
#define SDA_L       P1OUT&=~BIT4
#define SDA_H       P1OUT|=BIT4
#define SDA_IN      P1OUT|=BIT4; P1DIR&=~BIT4; P1REN|=BIT4
#define SDA_OUT     P1DIR|=BIT4; P1REN&=~BIT4
#define DAC6571_code_max      1023  // 10位ADC
#define DAC6571_address         0x98  // 1001 10 A0 0  A0=0


//////////////////////////////
//       变量定义           //
//////////////////////////////

// 软件定时器计数
unsigned char clock500ms=0;
// 软件定时器溢出标志
unsigned char clock500ms_flag=0;
// 测试用计数器
unsigned char digit[8]={' ',' ',' ',' ',' ',' ',' ',' '};
// 8位小数点 1亮  0灭
// 注：板上数码位小数点从左到右序号排列为4、5、6、7、0、1、2、3
unsigned char pnt=0x00;
// 8个LED指示灯状态，每个灯4种颜色状态，0灭，1绿，2红，3橙（红+绿）
// 注：板上指示灯从左到右序号排列为7、6、5、4、3、2、1、0
//     对应元件LED8、LED7、LED6、LED5、LED4、LED3、LED2、LED1
unsigned char led[]={0,0,1,1,2,2,3,3};
// 当前按键值
unsigned char key_code=0;
unsigned char key_cnt=0;

// DAC6571
int dac6571_code=(DAC6571_code_max>>2);
unsigned char dac6571_flag=0;

//////////////////////////////
//       系统初始化         //
//////////////////////////////

//  I/O端口和引脚初始化
void Init_Ports(void)
{
	P2SEL &= ~(BIT7+BIT6);       //P2.6、P2.7 设置为通用I/O端口
	  //因两者默认连接外晶振，故需此修改

	P2DIR |= BIT7 + BIT6 + BIT5; //P2.5、P2.6、P2.7 设置为输出
	  //本电路板中三者用于连接显示和键盘管理器TM1638，工作原理详见其DATASHEET

	P1DIR |= BIT4 + BIT5;
	P1OUT |= BIT4 + BIT5;
 }

//  定时器TIMER0初始化，循环定时20ms
void Init_Timer0(void)
{
	TA0CTL = TASSEL_2 + MC_1 ;      // Source: SMCLK=1MHz, UP mode,
	TA0CCR0 = 20000;                // 1MHz时钟,计满20000次为 20ms
	TA0CCTL0 = CCIE;                // TA0CCR0 interrupt enabled
}

//  MCU器件初始化，注：会调用上述函数
void Init_Devices(void)
{
	WDTCTL = WDTPW + WDTHOLD;     // Stop watchdog timer，停用看门狗
	if (CALBC1_8MHZ ==0xFF || CALDCO_8MHZ == 0xFF)
	{
		while(1);            // If calibration constants erased, trap CPU!!
	}

    //设置时钟，内部RC振荡器。     DCO：8MHz,供CPU时钟;  SMCLK：1MHz,供定时器时钟
	BCSCTL1 = CALBC1_8MHZ; 	 // Set range
	DCOCTL = CALDCO_8MHZ;    // Set DCO step + modulation
	BCSCTL3 |= LFXT1S_2;     // LFXT1 = VLO
	IFG1 &= ~OFIFG;          // Clear OSCFault flag
	BCSCTL2 |= DIVS_3;       //  SMCLK = DCO/8

    Init_Ports();           //调用函数，初始化I/O口
    Init_Timer0();          //调用函数，初始化定时器0
    _BIS_SR(GIE);           //开全局中断
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
//      中断服务程序        //
//////////////////////////////

// Timer0_A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void)
{
 	// 0.5秒钟软定时器计数
	if (++clock500ms>=V_T500ms)
	{
		clock500ms_flag = 1; //当0.5秒到时，溢出标志置1
		clock500ms = 0;
	}

	// 刷新全部数码管和LED指示灯
	TM1638_RefreshDIGIandLED(digit,pnt,led);

	// 检查当前键盘输入，0代表无键操作，1-16表示有对应按键
	//   键号显示在两位数码管上
	key_code=TM1638_Readkeyboard();
	if (key_code != 0)
	{
		if (key_cnt<3) key_cnt++;
		else if (key_cnt==3)
		{
			switch (key_code)
			{
			case 2: dac6571_code+=100;dac6571_flag=1;break; //2键加100
			case 6: dac6571_code-=100;dac6571_flag=1;break; //6键减100
			case 3: dac6571_code+= 10;dac6571_flag=1;break; //3键加10
			case 7: dac6571_code-= 10;dac6571_flag=1;break; //7键减10
			case 4: dac6571_code++;   dac6571_flag=1;break; //4键加1
			case 8: dac6571_code--;   dac6571_flag=1;break; //8键减1
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
//         主程序           //
//////////////////////////////

int main(void)
{
	unsigned char i=0;
	float temp;
	Init_Devices( );
	while (clock500ms<1);   // 延时足够时间等待TM1638上电完成
	init_TM1638();	    //初始化TM1638
	dac6571_flag = 1;

	while(1)
	{
		if (dac6571_flag==1)   // 检查DAC电压是否要变
		{
			dac6571_flag=0;

			digit[0] = dac6571_code/1000%10; 	//计算千位 0-1023
			digit[1] = dac6571_code/100%10; 	//计算百位
			digit[2] = dac6571_code/10%10;      //计算十位
			digit[3] = dac6571_code%10;      //计算各位

			dac6571_fastmode_operation();
		}


		if (clock500ms_flag==1)   // 检查0.5秒定时是否到
		{
			clock500ms_flag=0;
			// 8个指示灯以走马灯方式，每0.5秒向右（循环）移动一格
			temp=led[0];
			for (i=0;i<7;i++) led[i]=led[i+1];
			led[7]=temp;
		}
	}
}
