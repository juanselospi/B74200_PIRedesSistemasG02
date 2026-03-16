# Definicion del protocolo de comunicación

El protocolo que propongo para la comunicación entre procesos se basa en un *modelo cliente-servidor*. En el modelo, un proceso cliente le envía solicitudes al servidor para pedir algún servicio o alguna información, y el servidor responde con el resultado. La comunicación se va a organizar por solicitud y respuesta, donde cada mensaje se procesa según su prioridad.

Los mensajes del protocolo van a estar definidos mediante un struct que contiene varios campos importantes. Entre estos se incluyen un identificador(*ID*) único del mensaje, el tipo de mensaje(*solicitud, respuesta o error*), la identificación del proceso emisor del mensaje y la del destinatario. Además, cada mensaje va a incluir los datos que se desea enviar, también tendré un nivel de prioridad que ayude a determinar el orden en que deben procesarse los mensajes, y un indicador que especifica si el mensaje requiere una respuesta del receptor.

Para asegurar que esta comunicación sea confiable, mi protocolo también incluye mecanismos de confirmación y control de errores simples. En estos casos cuando se envíe un mensaje importante, el receptor debe enviar una confirmación de recepción. Si esta confirmación o la respuesta esperada no llega dentro de un tiempo determinado, se utilizará un temporizador(*timer*) para detectar un posible fallo y tomar las acciones necesarias, como reenviar el mensaje o reportar el error.

En general, la idea de este protocolo es proporcionar una forma organizada y fiable para que los procesos/hilos puedan comunicarse entre sí, manteniendo control sobre el orden de los mensajes, la identificación de los participantes y el manejo de errores durante la transmisión.
