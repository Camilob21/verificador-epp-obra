# Se compila el MISMO archivo C que usa el daemon; ninguna copia paralela.
idf_component_register(
    SRCS "../../../../common/protocolo/protocolo.c"
    INCLUDE_DIRS "../../../../common/protocolo"
)
