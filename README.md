# Cliente FTP Concurrente - BautistaA-clienteFTP

Cliente FTP implementado en C que soporta operaciones concurrentes mediante procesos fork. Permite realizar múltiples transferencias de archivos simultáneamente (hasta 5 transferencias concurrentes por defecto).

## Características

- **Transferencias concurrentes**: Soporta hasta 5 transferencias simultáneas de archivos
- **Modo pasivo**: Utiliza modo pasivo (PASV) por defecto para compatibilidad con firewalls
- **Manejo robusto de señales**: Implementa `sigaction()` para control de procesos hijo
- **Comandos FTP estándar**: Soporta comandos básicos de FTP como STOR, RETR, PWD, MKD, DELE

## Requisitos

- Sistema operativo Linux/Unix con soporte POSIX (Recomendado: Ubuntu)
- Compilador GCC
- Make

## Compilación

El proyecto incluye un `Makefile` para facilitar la compilación. Para compilar el cliente FTP, ejecute:

```bash
make
```

Esto generará el ejecutable `BautistaA-clienteFTP` a partir de los archivos fuente. El Makefile compilará automáticamente todos los archivos necesarios:

- `BautistaA-clienteFTP.c` (código principal del cliente)
- `connectsock.c`
- `connectTCP.c`
- `errexit.c`

Para limpiar los archivos generados:

```bash
make clean
```

Nota: Es necesario tener los archivos `connectsock.c`, `connectTCP.c` y `errexit.c`

## Uso

### Iniciar el cliente

Para conectarse a un servidor FTP, ejecute:

```bash
./BautistaA-clienteFTP <host> <puerto>
```

**Ejemplo:**

```bash
./BautistaA-clienteFTP localhost 21
```

> Para pruebas se hizo uso de vsftpd en Ubuntu.

### Inicio de sesión

Una vez conectado, debe autenticarse en el servidor. Existen dos formas:

**Opción 1: Comando único**

```
ftp> login <usuario> <contraseña>
```

**Opción 2: Comandos separados**

```
ftp> user <usuario>
ftp> pass <contraseña>
```

**Ejemplo:**

```
ftp> login anonymous password
```

### Comandos disponibles

| Comando                 | Descripción                            | Ejemplo                    |
| ----------------------- | -------------------------------------- | -------------------------- |
| `login <user> <pass>`   | Iniciar sesión en el servidor          | `login usuario contraseña` |
| `user <username>`       | Enviar nombre de usuario               | `user anonymous`           |
| `pass <password>`       | Enviar contraseña                      | `pass mi_pass`             |
| `stor <local> [remoto]` | Subir archivo al servidor              | `stor archivo.txt`         |
| `put <local> [remoto]`  | Alias de stor                          | `put documento.pdf`        |
| `retr <remoto> [local]` | Descargar archivo del servidor         | `retr archivo.txt`         |
| `get <remoto> [local]`  | Alias de retr                          | `get documento.pdf`        |
| `pwd`                   | Mostrar directorio actual del servidor | `pwd`                      |
| `mkd <dirname>`         | Crear directorio en el servidor        | `mkd nueva_carpeta`        |
| `dele <filename>`       | Eliminar archivo del servidor          | `dele archivo.txt`         |
| `passive`               | Alternar modo pasivo ON/OFF            | `passive`                  |
| `status`                | Ver transferencias activas             | `status`                   |
| `cleanup`               | Limpiar procesos terminados            | `cleanup`                  |
| `help`                  | Mostrar lista de comandos              | `help`                     |
| `quit` / `exit`         | Cerrar conexión y salir                | `quit`                     |

### Ejemplos de uso

**Subir un archivo:**

```
ftp> stor mi_documento.txt
```

**Descargar un archivo con nombre diferente:**

```
ftp> retr archivo_remoto.txt archivo_local.txt
```

**Verificar transferencias activas:**

```
ftp> status
Transferencias activas: 2/5
PIDs activos: 12345 12346
```

**Crear directorio y navegación:**

```
ftp> mkd documentos
ftp> pwd
257 "/home/user" is current directory
```

## Funcionamiento técnico

### Arquitectura concurrente

El cliente utiliza procesos fork para realizar transferencias de archivos en segundo plano. Cuando se ejecuta un comando `stor` o `retr`, el proceso principal:

1. Crea un proceso hijo mediante `fork()`
2. El hijo establece la conexión de datos y realiza la transferencia
3. El padre registra el PID del hijo y continúa aceptando comandos
4. Los procesos terminados se limpian automáticamente mediante señales SIGCHLD

### Manejo de señales

El cliente implementa un manejo robusto de señales usando `sigaction()`:

- **SIGCHLD**: Recolecta procesos hijo terminados automáticamente
- **SIGPIPE**: Ignora errores de conexiones rotas para evitar crashes
- Flags utilizados: `SA_SIGINFO`, `SA_RESTART`, `SA_NOCLDSTOP`

### Modo pasivo

El cliente utiliza modo pasivo (PASV) por defecto, que permite funcionar correctamente detrás de firewalls y NAT. En modo pasivo:

1. El cliente solicita al servidor entrar en modo pasivo
2. El servidor responde con una IP y puerto donde escucha
3. El cliente inicia la conexión de datos hacia ese puerto
4. Se realiza la transferencia

El modo pasivo puede desactivarse con el comando `passive`.

## Protocolo FTP

Este cliente implementa comandos del protocolo FTP estándar definido en RFC 959. Para más información sobre el protocolo FTP, consulte:

**RFC 959 - File Transfer Protocol (FTP):**  
https://datatracker.ietf.org/doc/html/rfc959

## Estructura del proyecto

```
BautistaA-clienteFTP/
├── BautistaA-clienteFTP.c    # Código principal del cliente FTP
├── connectsock.c              # Funciones de conexión de socket
├── connectTCP.c               # Funciones de conexión TCP
├── errexit.c                  # Funciones de manejo de errores
├── Makefile                   # Script de compilación
└── README.md                  # Este archivo
```

## Notas técnicas

- **Límite de concurrencia**: Por defecto, el cliente soporta hasta 5 transferencias simultáneas (`MAX_CONCURRENT = 5`)
- **Buffer de datos**: Utiliza un buffer de 8192 bytes para transferencias de archivos
- **Timeout**: No implementa timeouts automáticos, las conexiones permanecen abiertas hasta completarse o fallar

## Solución de problemas

**Error: "Máximo de transferencias concurrentes alcanzado"**

- Use el comando `status` para ver transferencias activas
- Use el comando `cleanup` para limpiar procesos terminados
- Espere a que finalicen las transferencias en curso

**Error al abrir archivo local**

- Verifique que el archivo existe en el directorio actual
- Verifique permisos de lectura/escritura
- Use rutas absolutas si es necesario

**Conexión rechazada**

- Verifique que el servidor FTP esté ejecutándose
- Confirme el puerto correcto (por defecto 21)
- Verifique reglas de firewall

## Autor

Bautista Alexis
