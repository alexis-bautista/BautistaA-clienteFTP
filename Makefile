# Nombre del compilador
CC = gcc

# Archivos fuente
SRC = BautistaA-clienteFTP.c connectsock.c connectTCP.c errexit.c

# Archivos objeto (se generan automáticamente a partir de los .c)
OBJ = $(SRC:.c=.o)

# Nombre del ejecutable
TARGET = BautistaA-clienteFTP

# Regla principal: compilar el programa
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Regla para limpiar archivos generados
clean:
	rm -f $(OBJ) $(TARGET)
