/*
 *  CMU 18-648 Embedded Real-Time Systems
 *  Lab 1 Team 6
 *    
 *  Calculator Syscall
 *  A implementaion of IEEE754 32bit floating point arithmetic
 */


#include <linux/types.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/syscalls.h>

/*
 * Integer types
 */
#define u64 uint64_t
#define u32 uint32_t
#define BIT_PER_BYTE 8

/*
 * IEEE754 single percision format
 */
#define EXP_MASK 0x7f800000
#define FRAC_MASK 0x007fffff
#define SIGN_MASK 0x80000000

#define EXP_SHIFT 23
#define FRAC_SHIFT 0
#define SIGN_SHIFT 31

#define EXP_DIGIT 8
#define FRAC_DIGIT 23
#define SIGN_DIGIT 1

#define EXP_MIN  0
#define EXP_MAX  ((1 << EXP_DIGIT) - 1)  
#define EXP_BIAS 127



/*
 * Function for doing float arithmatic in kernel
 * n1, n2: input number, which is in IEEE754 32 format
 * op: the operation charactor
 */
SYSCALL_DEFINE3(calc, u32, n1, u32, n2, char, op)
{   
    u32 exp1 = ((n1 & EXP_MASK) >> EXP_SHIFT) - EXP_BIAS;
    u32 frac1 = ((n1 & FRAC_MASK) >> FRAC_SHIFT) | (1 << FRAC_DIGIT);
    u32 sign1 = (n1 & SIGN_MASK) >> SIGN_SHIFT;
    u32 exp2 = ((n2 & EXP_MASK) >> EXP_SHIFT) - EXP_BIAS;
    u32 frac2 = ((n2 & FRAC_MASK) >> FRAC_SHIFT) | (1 << FRAC_DIGIT);
    u32 sign2 = (n2 & SIGN_MASK) >> SIGN_SHIFT;
    
    u32 exp = 0;
    u32 frac = 0;
    u32 sign = 0;
    u32 pos = 0;

    //if (n1 & SIGN_MASK || n2 & SIGN_MASK) return EINVAL;               //we do support negative number
    if (op != '+' && op != '-' && op != '*' && op != '/') return EINVAL; //check operation

    //compute frac and exp seperately
    if (op == '+' || op == '-') {
        //align fraction
        if (exp1 > exp2) {
            frac2 = frac2 >> (exp1 - exp2);
            exp = exp1;
        }
        else {              
            frac1 = frac1 >> (exp2 - exp1);
            exp = exp2;
        }
        
        //add and sub, check for each sign combination
        if (op == '+') {
            if (sign1 && sign2) {
                frac = frac1 + frac2;
                sign = 1;
            }
            else if (!sign1 && !sign2) {
                frac = frac1 + frac2;
                sign = 0;
            }
            else if (sign1 && !sign2) {
                if (frac1 > frac2) {
                    frac = frac1 - frac2;
                    sign = 1;
                }
                else {
                    frac = frac2 - frac1;
                    sign = 0;
                }
            }
            else if (!sign1 && sign2) {
                if (frac1 > frac2) {
                    frac = frac1 - frac2;
                    sign = 0;
                }
                else {
                    frac = frac2 - frac1;
                    sign = 1;
                }
            }
        }
        else if (op == '-'){
            if (!sign1 && sign2) {
                frac = frac1 + frac2;
                sign = 0;
            }
            else if (sign1 && !sign2) {
                frac = frac1 + frac2;
                sign = 1;
            }
            else if (sign1 && sign2) {
                if (frac1 > frac2) {
                    frac = frac1 - frac2;
                    sign = 1;
                }
                else {
                    frac = frac2 - frac1;
                    sign = 0;
                }
            }
            else if (!sign1 && !sign2) {
                if (frac1 > frac2) {
                    frac = frac1 - frac2;
                    sign = 0;
                }
                else {
                    frac = frac2 - frac1;
                    sign = 1;
                }
            }
        }
    }
    else if (op == '*') {
        u64 tmp_frac = (u64)frac1 * (u64)frac2; //use 64 bit to fit the result
        frac = tmp_frac >> FRAC_DIGIT;
        exp = exp1 + exp2;
        sign = sign1 ^ sign2;
    }
    else if (op == '/') {
        if (frac2 == (1 << FRAC_DIGIT)) return EINVAL;
        //losing percision because no 64 bit division
        //TODO: maybe implement one if there is time
        frac = (frac1 << 8) / (frac2 >> 15); //losing 15 bits at divisor 
        exp = exp1 - exp2;
        sign = sign1 ^ sign2;
    }
    
    //reconstruct the float format
    //find leading 1 pos
    for (pos = sizeof(u32)*BIT_PER_BYTE; pos >= 0; pos--) {
        if (frac & (1 << pos)) break;
    }
    //shift frac to fit in the format and update exp
    if (pos > FRAC_DIGIT) {
        int shift = pos - FRAC_DIGIT;
        frac = frac >> shift;
        exp += shift;
    }
    else {
        int shift = FRAC_DIGIT - pos;
        frac = frac << shift;
        exp -= shift;
    }

    frac &= ~(1 << FRAC_DIGIT); //remove the leading 1

    exp += EXP_BIAS; //add bias back

    //validate
    if (exp < 0 || exp > EXP_MAX) {
        return EINVAL;  //overflow
    }
    else if (exp == EXP_MAX && frac != 0){
        return EINVAL;  //not a number
    }

    return ((sign << SIGN_SHIFT) & SIGN_MASK) | ((frac << FRAC_SHIFT) & FRAC_MASK) | ((exp << EXP_SHIFT) & EXP_MASK);
}
