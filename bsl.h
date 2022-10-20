#ifndef BSLOAD_H_
#define BSLOAD_H_
    #define BSL_CONFIG_SIGNATURE (0x695A) // BSL Configuration Signature
    #define BSL_CONFIG_PASSWORD (0x5A00) // BSL User's Configuration Signature
    #define BSL_CONFIG_TINY_RAM_ERASED_BY_INIT (0xFFF7) // Tiny RAM is erased during BSL invocation
    #define BSL_CONFIG_TINY_RAM_NOT_ERASED_BY_INIT (0xFFFF) // (Default) Tiny RAM is not erased during BSL invocation
    #define BSL_CONFIG_RAM_ERASED_BY_INIT (0xFFFB) // RAM is erased during BSL invocation
    #define BSL_CONFIG_RAM_NOT_ERASED_BY_INIT (0xFFFF) // (Default) RAM is not erased during BSL invocation
    #define BSL_CONFIG_INTERFACE_UART_I2C (0xFFFC) // Enable UART and I2C
    #define BSL_CONFIG_INTERFACE_UART_ONLY (0xFFFD) // Enable UART
    #define BSL_CONFIG_INTERFACE_I2C_ONLY (0xFFFE) // Enable I2C

    void invokeBsl(void);

#endif
