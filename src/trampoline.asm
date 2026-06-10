[BITS 64]
section .text

global ExecuteWithTrapFlag
global _ExecuteWithTrapFlag

ExecuteWithTrapFlag:
_ExecuteWithTrapFlag:
    
    pushfq
    pop rax
    bts rax, 8         
    push rax
    popfq
    
    call rcx
    
    pushfq
    pop rax
    btr rax, 8          
    push rax
    popfq
    
    ret