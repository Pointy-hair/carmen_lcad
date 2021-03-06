#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include "imu.h"
#include "bmp180.h"
#include <time.h>

#define PORT 3457
#define SOCKET_DATA_PACKET_SIZE	2048

int server_fd;
int file;

void
extract_camera_configuration(char *cam_config, int &image_width, int &image_height, int &frame_rate, int &brightness, int &contrast)
{
	char *token;

	token = strtok(cam_config, "*");

	printf ("--- Connected! Widith: %s ", token);
	image_width = atoi(token);

	token = strtok (NULL, "*");
	printf ("Height: %s ", token);
	image_height = atoi(token);

	token = strtok (NULL, "*");
	printf ("Frame Rate: %s ", token);
	frame_rate = atoi(token);

	token = strtok (NULL, "*");
	printf ("Brightness: %s ", token);
	brightness = atoi(token);

	token = strtok (NULL, "*");
	printf ("Contrast: %s ---\n", token);
	contrast = atoi(token);
}


void
set_imu_configurations()
{
	file = detectIMU();
	enableIMU();
}


int
connect_with_client()
{
    int new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("--- Socket Failed ---\n");
        return (-1);
    }
    //printf("--- OPT\n");
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("--- Setsockopt Failed ---\n");
        return (-1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
    
    //printf("--- BIND\n");
    // Forcefully attaching socket to the port defined
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        perror("--- Bind Failed ---\n");
        return (-1);
    }
    //printf("--- Listen\n");
    if (listen(server_fd, 3) < 0)
    {
        perror("-- Listen Failed ---\n");
        return (-1);
    }
    
    printf("--- Waiting for connection! --\n");
    if ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0)
    {
        perror("--- Accept Failed ---\n");
        return (-1);
    }
    printf("--- Connection established successfully! ---\n");

    return (new_socket);
}

double
get_time(void)
{
  struct timeval tv;
  double t;

  if (gettimeofday(&tv, NULL) < 0)
    printf("time error\n");
  t = tv.tv_sec + tv.tv_usec/1000000.0;
  return t;
}

int
main()
{
	int pi_socket = connect_with_client();

	set_imu_configurations();

	double start, end;

	int magRaw[3];
	int accRaw[3];
	int gyrRaw[3];

	char status;

	double temperature = 0.0;
	double pressure = 0.0;

	while (1)
	{
		start = get_time();
		unsigned char rpi_imu_data[SOCKET_DATA_PACKET_SIZE];

		readACC(accRaw);
		readGYR(gyrRaw);
		readMAG(magRaw);
		//readCalBMP180(file);

		/*status = startTemperature(file);
		if (status != 0)
		{
			usleep(status * 1000);
			getTemperature(temperature, file);
		}

		status = startPressure(3, file);
		if (status != 0)
		{
			// Wait for the measurement to complete:
			usleep(status * 1000);
			status = getPressure(pressure, temperature, file);
		}
*/
		sprintf((char *) rpi_imu_data, "%d %d %d %d %d %d %d %d %d *\n", accRaw[0], accRaw[1], accRaw[2], gyrRaw[0], gyrRaw[1], gyrRaw[2],
				magRaw[0], magRaw[1], magRaw[2]);

		int result = send(pi_socket, rpi_imu_data, SOCKET_DATA_PACKET_SIZE, MSG_NOSIGNAL);  // Returns number of bytes read, 0 in case of connection lost, -1 in case of error
		if (result == -1)
		{
			printf("--- Disconnected ---\n");
            close(server_fd);
            sleep(3);
            
            pi_socket = connect_with_client();
        }

		usleep(7000);
		end = get_time();
		printf("%f Hz\n", 1 / (end - start));
    }

   return (0);
}
