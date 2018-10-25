#include <vector>
#include <iostream>
#include <fstream>
#include <math.h>

extern "C" {
    #include <stdio.h>
    #include <stdlib.h> // for atoi
	#include <getopt.h>
	#include <signal.h>
	#include <rc/mpu.h>
	#include <rc/math.h>
	#include <rc/button.h>
	#include <rc/encoder.h>
	#include <rc/time.h>
	#include <rc/motor.h>
	#include <rc/pthread.h>
	#include <rc/spi.h>
	#include <rc/servo.h>
	#include <rc/adc.h>
}

using namespace std;

#define PI 3.14159265f
[[gnu::unused]] static void __signal_handler( [[gnu::unused]] int dummy);

/*-------------definicoes do SPI------------*/


#define CANAL_CHASSIS 0
#define CANAL_GARRA 1

#define BARRAMENTO_CHASSIS RC_BB_SPI1_SS1
#define BARRAMENTO_GARRA RC_BB_SPI1_SS2

#define SLAVE_MODE	SPI_SLAVE_MODE_AUTO
#define BUS_MODE	SPI_MODE_0
#define SPI_SPEED	100000
string SpiComm(int canal, char* dado, int tamanho_resposta);


/*---------definicoes dos sensores----------*/

#define QUANTIDADE_LDR 8
#define QUANTIDADE_COR 4
#define TAMANHO_RESPOSTA_CHASSIS 16
#define SENSORES_LDR 1
#define SENSORES_COR 2
vector<uint8_t> sensoresLDR(8);
vector<uint8_t> sensoresCOR(4);

void imprimeLeituras(int sensores);
void lerSensoresChassis();

/*---------definicoes signal handling----------*/
static int running;



/*------------definicoes motores---------------*/
#define MOTOR_FRENTE 1
#define MOTOR_TRAS 2
#define MOTOR_DIREITA 3
#define MOTOR_ESQUERDA 4
#define TODOS_OS_MOTORES 0

#define DUTY_CYCLE_MAXIMO 0.725f//normaliza potencia
#define NORMALIZA_POTENCIA DUTY_CYCLE_MAXIMO/100

#define DIRECAO_LADO_Y 1
#define DIRECAO_SENSORES_X 2

#define QUANTIDADE_PULSOS_POR_REV 350
#define QUANTIDADE_PULSOS_PRECISAO 5
#define DIAMETRO_DA_RODA 11

/*------------definicoes locomocao-------------*/
#define X_POS 1
#define X_NEG 2
#define Y_POS 3
#define Y_NEG 4


void andaMotores(int direcao, float vel);
int inicializa();
void andaDistancia(float dist, int eixo);
bool acabouDeAndar(long long int cont1, long long int cont2, float dist);

vector<float> orientacao(3);


/*-----------definicoes mpu--------------------*/
// bus for Robotics Cape and BeagleboneBlue is 2, interrupt pin is on gpio3.21
// change these for your platform
#define I2C_BUS 2
#define GPIO_INT_PIN_CHIP 3
#define GPIO_INT_PIN_PIN  21
static rc_mpu_data_t data;

void __atualizaOrientacao()
{
	orientacao[0] = data.dmp_TaitBryan[TB_PITCH_X]*RAD_TO_DEG;
	orientacao[1] = data.dmp_TaitBryan[TB_ROLL_Y]*RAD_TO_DEG;
	orientacao[2] = data.dmp_TaitBryan[TB_YAW_Z]*RAD_TO_DEG;
}

int main () 
{
	//vector<char> dados = {'x','l'};
	//char* dados = "s";
	if (inicializa())
	{
		cout << "deu ruim na inicializacao"<<endl;
		return 0;
	}
			
	
	string retorno = string(SpiComm(0,(char *)"s",16));
	cout << retorno <<endl<<retorno.length()<<endl;
	
	lerSensoresChassis();
	imprimeLeituras(SENSORES_LDR);
	imprimeLeituras(SENSORES_COR);


	while(running)
	{
		rc_usleep(500000);
		//cout<<orientacao[0]<<"  "<<orientacao[1]<<"  "<<orientacao[2]<<endl;
	}

	return 0;
}


int inicializa()
{
	signal(SIGINT, __signal_handler);
	running = 1;
	system("config-pin p9.30 spi");
	system("config-pin p9.31 spi_sclk");
	system("config-pin p9.28 spi_cs");
	system("config-pin p9.29 spi");
	cout << "Spi pins activated" << endl;
	
	rc_mpu_config_t config = rc_mpu_default_config();
	config.i2c_bus = I2C_BUS;

	if(rc_mpu_calibrate_gyro_routine(config)<0){
		cout << "Failed to complete gyro calibration\n";
		return -1;
	}
	cout << "Gyro Calibrated." << endl;
	
	config.gpio_interrupt_pin_chip = GPIO_INT_PIN_CHIP;
	config.gpio_interrupt_pin = GPIO_INT_PIN_PIN;
	
	if(rc_mpu_initialize_dmp(&data, config)){
		printf("rc_mpu_initialize_failed\n");
		return -1;
	}
	
	rc_mpu_set_dmp_callback(&__atualizaOrientacao);

	rc_motor_init_freq(RC_MOTOR_DEFAULT_PWM_FREQ);
	rc_encoder_init();
	return 0;
}

bool acabouDeAndar(long long int cont1, long long int cont2, float dist)
{
	return ((abs(cont1-cont2)< QUANTIDADE_PULSOS_PRECISAO) && cont1>=round(dist*QUANTIDADE_PULSOS_POR_REV/DIAMETRO_DA_RODA))? true : false;
}

void andaDistancia(float dist, float pot,int eixo)
{
	long long int cont_inicial_1 = 0;
	long long int cont_inicial_2 = 0;
	switch(eixo)
	{
		case X_POS:
			cont_inicial_1 = rc_encoder_read(MOTOR_FRENTE);
			cont_inicial_2 = rc_encoder_read(MOTOR_TRAS);
			andaMotores(DIRECAO_SENSORES_X,pot);
			while(!acabouDeAndar(cont_inicial_1,cont_inicial_2,dist))
			{
				cont_inicial_1 = rc_encoder_read(MOTOR_FRENTE);
				cont_inicial_2 = rc_encoder_read(MOTOR_TRAS);
			}
			break;
		case X_NEG:
			cont_inicial_1 = rc_encoder_read(MOTOR_FRENTE);
			cont_inicial_2 = rc_encoder_read(MOTOR_TRAS);
			andaMotores(DIRECAO_SENSORES_X,-pot);
			while(!acabouDeAndar(cont_inicial_1,cont_inicial_2,dist))
			{
				cont_inicial_1 = rc_encoder_read(MOTOR_FRENTE);
				cont_inicial_2 = rc_encoder_read(MOTOR_TRAS);
			}

			break;
		case Y_POS:
			cont_inicial_1 = rc_encoder_read(MOTOR_DIREITA);
			cont_inicial_2 = rc_encoder_read(MOTOR_ESQUERDA);
			andaMotores(DIRECAO_LADO_Y,pot);
			while(!acabouDeAndar(cont_inicial_1,cont_inicial_2,dist))
			{
				cont_inicial_1 = rc_encoder_read(MOTOR_DIREITA);
				cont_inicial_2 = rc_encoder_read(MOTOR_ESQUERDA);
			}
			break;
		case Y_NEG:
			cont_inicial_1 = rc_encoder_read(MOTOR_DIREITA);
			cont_inicial_2 = rc_encoder_read(MOTOR_ESQUERDA);
			andaMotores(DIRECAO_LADO_Y,-pot);
			while(!acabouDeAndar(cont_inicial_1,cont_inicial_2,dist))
			{
				cont_inicial_1 = rc_encoder_read(MOTOR_DIREITA);
				cont_inicial_2 = rc_encoder_read(MOTOR_ESQUERDA);
			}
			break;
		default:
			break;

	}

	rc_motor_brake(TODOS_OS_MOTORES);
}

void andaMotores(int direcao, float pot)
{
	if (direcao == DIRECAO_SENSORES_X)
	{
		rc_motor_set(MOTOR_DIREITA,pot*NORMALIZA_POTENCIA);
		rc_motor_set(MOTOR_ESQUERDA,pot*NORMALIZA_POTENCIA);
	}else if(direcao == DIRECAO_LADO_Y)
	{
		rc_motor_set(MOTOR_FRENTE,pot*NORMALIZA_POTENCIA);
		rc_motor_set(MOTOR_TRAS,pot*NORMALIZA_POTENCIA);
	}
}

string SpiComm(int canal, char* dado, int tamanho_resposta)
{
	char* test_str = dado;
	int bytes = tamanho_resposta;	// get number of bytes in test string
	uint8_t buf[bytes];		// read buffer
	int ret;			// return value
	// attempt a string send/receive test
	printf("Sending  %d bytes: %s\n", bytes, test_str);
	if(canal == CANAL_CHASSIS){
		if(rc_spi_init_auto_slave(BARRAMENTO_CHASSIS, BUS_MODE, SPI_SPEED)){
			string vec = {'F'};
			return vec;
		}
	
		ret = rc_spi_transfer(BARRAMENTO_CHASSIS, (uint8_t*)test_str, bytes, buf);
	}else if(canal == CANAL_GARRA){
		if(rc_spi_init_auto_slave(BARRAMENTO_GARRA, BUS_MODE, SPI_SPEED)){
			string vec = {'F'};
			return vec;
		}
		ret = rc_spi_transfer(BARRAMENTO_GARRA, (uint8_t*)test_str, bytes, buf);
	}
	if(ret<0){
		printf("send failed\n");
		rc_spi_close(canal);
		string vec = {'F'};
		return vec;
	}
	else printf("Received %d bytes: %s\n",ret, buf);
	string vec = string((char *)buf);
	rc_spi_close(1);
	return vec;
}

void lerSensoresChassis()
{
	//char* tag = "s";
	string aux = SpiComm(CANAL_CHASSIS,(char *)"s",TAMANHO_RESPOSTA_CHASSIS);
	int j = 0;
	for (int i = 0; (i < signed(aux.length())) && (j < (QUANTIDADE_LDR + QUANTIDADE_COR)); ++i)
	{
		if (aux[i] >= '1' && aux[i] <= ('1'+2))
		{
			if(j<QUANTIDADE_LDR){
				sensoresLDR[j] = aux[i] - '0';
				//printf("%c %d \n",aux[i],sensoresLDR[j]);
			}
			else{
				sensoresCOR[j- QUANTIDADE_LDR] = aux[i] - '0';
				//printf("%c %d j=%d \n",aux[i],sensoresCOR[j- QUANTIDADE_LDR],j);
			}
			j++;
		}
	}
}


void imprimeLeituras(int sensores)
{
	if (sensores == SENSORES_LDR)
	{
		for (int i = 0; i < signed(sensoresLDR.size()); ++i)
		{
			cout<<"sensorLDR "<<i<<"= "<<int(sensoresLDR[i])<<endl;
		}
	}else if(sensores == SENSORES_COR){
		for (int i = 0; i < signed(sensoresCOR.size()); ++i)
		{
			cout<<"sensorCOR "<<i<<"= "<<int(sensoresCOR[i])<<endl;
		}
	}
}

[[gnu::unused]] static void __signal_handler([[gnu::unused]] int dummy)//__attribute__ ((unused)) int dummy)
{
	running=0;
	return;
}
