#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include <sys/time.h>
#include <time.h>

typedef struct{
	long double iteracionesPrueba;
	long double llamadasMpi;
	double tCalculoPrueba;
	long double iteracionesTotales;
} Tipo_Datos_Entrada;

int funcion_calculador();
int funcion_IO(int, int, int);
double mygettime(void);
void Crea_Tipo_Derivado_Salida(Tipo_Datos_Entrada * pDatos, MPI_Datatype * pMPI_Tipo_Datos);

int main (int argc, char* argv[]) {
	int iId ; 
	int iNumProcs;
	
	MPI_Init (&argc ,&argv); 						
	MPI_Comm_rank(MPI_COMM_WORLD,&iId ); 			
	MPI_Comm_size(MPI_COMM_WORLD, &iNumProcs );

	if (argc < 3 || atoi(argv[1]) < 0 || atoi(argv[2]) < 0 || iNumProcs < 2) {
		if (iId == 0)
			perror("Parámetros incorrectos -> mpirun -np [Nº proc] ./practica [tTotal] [tActualizacion]");
		
	} else {
		if (iId == 0)
			funcion_IO(atoi(argv[1]), atoi(argv[2]), iNumProcs);
		else
			funcion_calculador();
	}

	MPI_Finalize(); 
	return 0;
}

int funcion_calculador() {
	int operacion, calculate = 1, actualizacionPedida, etiqueta = 50;
	float ldParam=0.5, ldResult;
	double tmpInicioPrueba;
	MPI_Request request;
	Tipo_Datos_Entrada datosCalculados;
	MPI_Datatype MPI_Tipo_Datos_Entrada;

	Crea_Tipo_Derivado_Salida(&datosCalculados,&MPI_Tipo_Datos_Entrada);

	//Los procesos se quedan esperando a que el proceso principal les mande la primera operacion
	MPI_Recv(&operacion,1, MPI_INT, 0, etiqueta, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	
	datosCalculados.iteracionesPrueba = 0.0L;
	datosCalculados.iteracionesTotales = 0.0L;
	datosCalculados.llamadasMpi = 0.0L;
	datosCalculados.tCalculoPrueba = 0;

	//Lo hacemos de esta forma para que sea más eficiente
	datosCalculados.llamadasMpi = 4;
	while(calculate){
		tmpInicioPrueba = mygettime();
		switch(operacion)
		{
			case 1:{
				ldResult=hypotl(ldParam,ldParam);
				break;} 
			case 2: {
				ldResult=cbrtl(ldParam);
				break;}
			case 3: {
				ldResult=erfl(ldParam);
				break;}
			case 4: {
				ldResult=sqrtl(ldParam);
				break;}
		}

		datosCalculados.iteracionesPrueba++;
		datosCalculados.iteracionesTotales++;

		datosCalculados.tCalculoPrueba += mygettime() - tmpInicioPrueba;
		
		//Los procesos comprueban si han recibido algun mensaje
		MPI_Iprobe(0, etiqueta, MPI_COMM_WORLD, &actualizacionPedida, MPI_STATUS_IGNORE);

		if (actualizacionPedida){
			//En el caso de que hayan recibido algun mensaje, comprobará el mensaje recibido
			MPI_Recv(&calculate,1, MPI_INT, 0, etiqueta, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			switch(calculate){
				case 0:
					//Si ha recibido un 0 significará que finaliza el programa
					break;

				case -1:
					//Si el mensaje es un -1, tendrá que actualizar los datos
					datosCalculados.llamadasMpi = (datosCalculados.iteracionesPrueba) + 3;
					MPI_Send(&datosCalculados, 1, MPI_Tipo_Datos_Entrada, 0, etiqueta, MPI_COMM_WORLD);
					break;

				case -2:
					MPI_Send(&datosCalculados, 1, MPI_Tipo_Datos_Entrada, 0, etiqueta, MPI_COMM_WORLD);

					datosCalculados.iteracionesPrueba = 0.0L;
					datosCalculados.llamadasMpi = 0.0L;
					datosCalculados.tCalculoPrueba = 0;

					MPI_Recv(&calculate,1, MPI_INT, 0, etiqueta, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					operacion = calculate;
					tmpInicioPrueba = mygettime();
					break;
			}
		}
	}
	
	MPI_Type_free(&MPI_Tipo_Datos_Entrada);
	return 0;
}

int funcion_IO(int tTotal, int tActualizacion, int iNumProcs) {
	int etiqueta = 50, destino, operacion, actualizar = -1;
	long double numIteraciones[4] = {0.0L, 0.0L, 0.0L, 0.0L};
	long double llamadasMpiTotales[4] = {0.0L, 0.0L, 0.0L, 0.0L};
	Tipo_Datos_Entrada datosCalculados;
	MPI_Datatype MPI_Tipo_Datos_Entrada;

	Crea_Tipo_Derivado_Salida(&datosCalculados,&MPI_Tipo_Datos_Entrada);

  datosCalculados.iteracionesPrueba = 0.0L;
	datosCalculados.iteracionesTotales = 0.0L;
	datosCalculados.llamadasMpi = 0.0L;
	datosCalculados.tCalculoPrueba = 0;

	for (operacion = 1; operacion < 5;operacion++){
		// Enviamos a todos la operacion a realizar
		printf("\n\t\t\t\t\t\t--- OPERACION %d ---\n", operacion);
		printf("\t%-11s%-25s%-18s%-30s%-15s\n","PROCESO", "Nº ITERACIONES", "Tº PRUEBA", "Nº TOTAL DE ITERACCIONES", "Nº DE LLAMADAS A MPI");
		
		for (int destino = 1; destino < iNumProcs; destino++)
			MPI_Send(&operacion, 1, MPI_INT, destino, etiqueta, MPI_COMM_WORLD);

		// Cada tiempo de actualización preguntaremos por el número de iteraciones
		for (int actualizacion = 0; actualizacion < floor(tTotal/tActualizacion); actualizacion++) {
			sleep(tActualizacion);

			// Enviar con un tag distinto para que no se mezcle con el de operacion
			for (int destino = 1; destino < iNumProcs; destino++)
				MPI_Send(&actualizar, 1, MPI_INT, destino, etiqueta, MPI_COMM_WORLD);

			for (int destino = 1; destino < iNumProcs; destino++){
				//Espera hasta que reciba los datos de actualización
				MPI_Recv(&datosCalculados,1, MPI_Tipo_Datos_Entrada, MPI_ANY_SOURCE, etiqueta, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				printf("\t%-11d%-25.0Lf%-18f%-30.0Lf%-15.0Lf\n", destino, datosCalculados.iteracionesPrueba, datosCalculados.tCalculoPrueba, datosCalculados.iteracionesTotales,datosCalculados.llamadasMpi);
			}
		}

		sleep(tTotal - (floor(tTotal/tActualizacion) * tActualizacion));

		//Enviamos el -2 para que sepa que debe enviar los datos totales de la operacion
		actualizar = -2;
		for (int destino = 1; destino < iNumProcs; destino++)
			MPI_Send(&actualizar, 1, MPI_INT, destino, etiqueta, MPI_COMM_WORLD);

		for (int destino = 1; destino < iNumProcs; destino++){
			MPI_Recv(&datosCalculados,1, MPI_Tipo_Datos_Entrada, MPI_ANY_SOURCE, etiqueta, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			printf("\t%-11d%-25.0Lf%-18f%-30.0Lf%-15.0Lf\n", destino, datosCalculados.iteracionesPrueba, datosCalculados.tCalculoPrueba, datosCalculados.iteracionesTotales,datosCalculados.llamadasMpi);
			numIteraciones[operacion - 1] += datosCalculados.iteracionesPrueba;
			llamadasMpiTotales[operacion -1] += datosCalculados.llamadasMpi;
		}

		//Ponemos actualizar de nuevo a -1
		actualizar = -1;
	}

	// Avisamos de que tienen que acabar enviando un 0
	operacion = 0;
	for (int destino = 1; destino < iNumProcs; destino++){
		MPI_Send(&operacion, 1, MPI_INT, destino, etiqueta, MPI_COMM_WORLD);
	}

	printf("\n\n\t\t\tINTERACCIONES TOTALES\n");

	for (int i = 0; i < 4; ++i)
		printf("\t\t\tOperacion %d\t Iteracciones-> %.0Lf\t Llamadas a MPI-> %.0Lf\n", i + 1, numIteraciones[i], llamadasMpiTotales[i]);

	MPI_Type_free(&MPI_Tipo_Datos_Entrada);
	return 0;
}

double mygettime(void) {
  struct timeval tv;
  
  if(gettimeofday(&tv, 0) < 0)
    perror("oops");
  
  return (double)tv.tv_sec + (0.000001 * (double)tv.tv_usec);
}

void Crea_Tipo_Derivado_Salida(Tipo_Datos_Entrada * pDatos, MPI_Datatype * pMPI_Tipo_Datos)
{
	MPI_Datatype tipos[4];
	int longitudes[4];
	MPI_Aint direcc[5];
	MPI_Aint desplaz[4];
	
	tipos[0]=MPI_LONG_DOUBLE;
	tipos[1]=MPI_LONG_DOUBLE;
	tipos[2]=MPI_DOUBLE;
	tipos[3]=MPI_LONG_DOUBLE;
	
	longitudes[0]=1;
	longitudes[1]=1;
	longitudes[2]=1;
	longitudes[3]=1;
	
	MPI_Get_address(pDatos, &direcc[0]);
	MPI_Get_address(&(pDatos->iteracionesPrueba), &direcc[1]);
	MPI_Get_address(&(pDatos->llamadasMpi), &direcc[2]);
	MPI_Get_address(&(pDatos->tCalculoPrueba), &direcc[3]);
	MPI_Get_address(&(pDatos->iteracionesTotales), &direcc[4]);
	
	desplaz[0]=direcc[1]-direcc[0];
	desplaz[1]=direcc[2]-direcc[0];
	desplaz[2]=direcc[3]-direcc[0];
	desplaz[3]=direcc[4]-direcc[0];
	
	MPI_Type_create_struct(4, longitudes, desplaz, tipos, pMPI_Tipo_Datos);
	
	MPI_Type_commit(pMPI_Tipo_Datos);
}