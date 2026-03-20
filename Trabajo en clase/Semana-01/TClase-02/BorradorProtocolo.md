# Interacción entre clientes y tenedores

La comunicación entre el cliente y el tenedor se basa en un protocolo de solicitud-respuesta usando un protocolo de transporte confiable como TCP. El cliente enviará una petición con formato de estructura que incluye un identificador de la figura de Lego y de ser necesario la mitad de la pieza requeridas. 
El tenedor actuará como intermediario validando la solicitud y gestionando la comunicación con el servidor. Para optimizar el proceso se pueden estructurar mensajes con el formato Json para estructurar los mensajes.

El control de errores mediante códigos de estado en casos inesperados, y fragmentación de datos en bloques de 256 bits en caso de que la lista de piezas sea muy grande. Además el tenedor puede implementar caché con un arbol para almacenar solicitudes frecuentes y reducir la carga al servidor. Una vez obtenida la información, el tenedor envía al cliente la lista completa de piezas necesarias para construir la figura solicitada.


# Solicitud del contenido del sistema de archivos

Para solicitar el contenido del sistema de archivos el cliente envía una petición al tenedor utilizando el protocolo de solicitud-respuesta TCP. Esta solicitud se va a estructurar en formato JSON e incluye el identificador de la figura que se desea consultar.
Internamente el sistema de archivos está organizado mediante una estructura de árbol, lo que permite representar de forma jerárquica los directorios y archivos, el tenedor recibe la petición, valida su formato y permisos, y navega dicha estructura para localizar el nodo correspondiente. En caso de no tener la información en caché, en ese caso realizará la consulta al servidor. En respuesta a eso, el tenedor enviará al cliente una lista estructurada con los elementos de la figura, incluyendo nombres, tipos, y metadatos básicos. Si la respuesta es muy grande, se fragmenta en bloques de 256 bits para asegurar una transmisión eficiente, y se incluyen códigos de estado para indicar éxito o posibles errores.


# Solicitud de una figura en particular

Para solicitar una figura específica el cliente enviará una petición al tenedor utilizando el protocolo de solicitud-respuesta TCP. La solicitud se estructura en formato JSON e incluye el identifador único de la figura de Lego requerida(así se maneja el caso donde hayan 2 fichas con el msimo nombre pero que sean distintas). El tenedor recibirá la petición, validará su formato y verificará en su sistema de caché si la información ya está disponible. En caso contrario, hará una consulta al servidor para obtener los datos asociados a la figura. Una vez encontrada, el tenedor enviará al cliente la lista de piezas necesarias con sus metadatos respectivos. Si la cantidad de datos es grande, la respuesta se fragmenta en bloques de 256 bits. Además, se utilizan códigos de estado para indicar si la solicitud fue exitosa o si ocurrió algún error, como por ejemplo: figura no encontrada.


# Interacción entre tenedores y servidores

La interacción entre el tenedor y el servidor también sigue un modelo de solicitud-respuesta por TCP. El tenedor actúa como cliente de cara al servidor enviando solicitudes estructuradas en formato JSON para obtener información que no se encuentra en su caché local. Estas solicitudes pueden incluir nombres de figuras, o en casio de requerir solo una parte de la figura sería la mitad corrspondiente. El servidor procesa la petición accediendo a su sistema de almacenamiento, organizado mediante una estructura de árbol, y responde con los datos solicitados junto con sus metadatos. Para garantizar eficiencia, las respuestas pueden fragmentarse en bloques de 256 bits si el tamaño es grande.
Para manejo de errores se emplean códigos de estado para indicar el resultado de la operación. Mi esquema está planteado de forma que permita mantener la separación de responsabilidades, donde el servidor gestiona los datos y el tenedor optimiza el acceso mediante validación, caché y control de solicitudes.

