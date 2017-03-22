#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y86asm.h"

line_t *y86bin_listhead = NULL;   /* the head of y86 binary code line list*/
line_t *y86bin_listtail = NULL;   /* the tail of y86 binary code line list*/
int y86asm_lineno = 0; /* the current line number of y86 assemble code */

#define err_print(_s, _a...) do { \
  if (y86asm_lineno < 0) \
    fprintf(stderr, "[--]: "_s"\n", ## _a); \
  else \
    fprintf(stderr, "[L%d]: "_s"\n", y86asm_lineno, ## _a); \
} while (0);

#define err_print0(_s ) do { \
  if (y86asm_lineno < 0) \
    fprintf(stderr, "[--]: "_s"\n"); \
    else \
    fprintf(stderr, "[L%d]: "_s"\n", y86asm_lineno); \
} while (0);

int vmaddr = 0;    /* vm addr */

/* register table */
reg_t reg_table[REG_CNT] = {
    {"%eax", REG_EAX},
    {"%ecx", REG_ECX},
    {"%edx", REG_EDX},
    {"%ebx", REG_EBX},
    {"%esp", REG_ESP},
    {"%ebp", REG_EBP},
    {"%esi", REG_ESI},
    {"%edi", REG_EDI},
};

regid_t find_register(char *name)
{
    int i=0;
    for(;i<REG_CNT;i++){
        if(!strcmp(reg_table[i].name,name))
            return reg_table[i].id;
    }
    return REG_ERR;
}

/* instruction set */
instr_t instr_set[] = {
    {"nop", 3,   HPACK(I_NOP, F_NONE), 1 },
    {"halt", 4,  HPACK(I_HALT, F_NONE), 1 },
    {"rrmovl", 6,HPACK(I_RRMOVL, F_NONE), 2 },
    {"cmovle", 6,HPACK(I_RRMOVL, C_LE), 2 },
    {"cmovl", 5, HPACK(I_RRMOVL, C_L), 2 },
    {"cmove", 5, HPACK(I_RRMOVL, C_E), 2 },
    {"cmovne", 6,HPACK(I_RRMOVL, C_NE), 2 },
    {"cmovge", 6,HPACK(I_RRMOVL, C_GE), 2 },
    {"cmovg", 5, HPACK(I_RRMOVL, C_G), 2 },
    {"irmovl", 6,HPACK(I_IRMOVL, F_NONE), 6 },
    {"rmmovl", 6,HPACK(I_RMMOVL, F_NONE), 6 },
    {"mrmovl", 6,HPACK(I_MRMOVL, F_NONE), 6 },
    {"addl", 4,  HPACK(I_ALU, A_ADD), 2 },
    {"subl", 4,  HPACK(I_ALU, A_SUB), 2 },
    {"andl", 4,  HPACK(I_ALU, A_AND), 2 },
    {"xorl", 4,  HPACK(I_ALU, A_XOR), 2 },
    {"jmp", 3,   HPACK(I_JMP, C_YES), 5 },
    {"jle", 3,   HPACK(I_JMP, C_LE), 5 },
    {"jl", 2,    HPACK(I_JMP, C_L), 5 },
    {"je", 2,    HPACK(I_JMP, C_E), 5 },
    {"jne", 3,   HPACK(I_JMP, C_NE), 5 },
    {"jge", 3,   HPACK(I_JMP, C_GE), 5 },
    {"jg", 2,    HPACK(I_JMP, C_G), 5 },
    {"call", 4,  HPACK(I_CALL, F_NONE), 5 },
    {"ret", 3,   HPACK(I_RET, F_NONE), 1 },
    {"pushl", 5, HPACK(I_PUSHL, F_NONE), 2 },
    {"popl", 4,  HPACK(I_POPL, F_NONE),  2 },
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1 },
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2 },
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4 },
    {".pos", 4,  HPACK(I_DIRECTIVE, D_POS), 0 },
    {".align", 6,HPACK(I_DIRECTIVE, D_ALIGN), 0 },
    {NULL, 1,    0   , 0 } //end
};

instr_t *find_instr(char *name)
{
    instr_t *ins=instr_set;
    while(ins!=NULL){
        if(!strcmp(ins->name,name))
            return ins;
        ins++;
    }
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{
    symbol_t *p=symtab->next;
    while(p!=NULL){
		if (!strcmp(p->name, name))
			return p;
		else
			p = p->next;
    }
    return NULL;
}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{    
    /* check duplicate */
    if(find_symbol(name)!=NULL)
        return -1;
    else{
         /* create new symbol_t (don't forget to free it)*/
        symbol_t *new;
        symbol_t *p=symtab;
        new=(symbol_t*)malloc(sizeof(symbol_t));
        new->name=name;
        new->next=NULL;
		new->addr = vmaddr;
        /* add the new symbol_t to symbol table */
		while (symtab->next != NULL)
			symtab = symtab->next;
		symtab->next = new;
		symtab = p;
    }
    return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_reloc(char *name, bin_t *bin)
{
    reloc_t *p=reltab;
    reloc_t *new;
    while(reltab->next!=NULL)
		reltab = reltab->next;
		
    /* create new reloc_t (don't forget to free it)*/
    new=(reloc_t*)malloc(sizeof(reloc_t));
    new->name=name;
    new->y86bin=bin;
	new->next = NULL;
    /* add the new reloc_t to relocation table */
	reltab->next = new;
	reltab = p;
    return 0;
}


/* macro for parsing y86 assembly code */
#define IS_DIGIT(s) ((*(s)>='0' && *(s)<='9') || *(s)=='-' || *(s)=='+')
#define IS_LETTER(s) ((*(s)>='a' && *(s)<='z') || (*(s)>='A' && *(s)<='Z'))
#define IS_COMMENT(s) (*(s)=='#')
#define IS_REG(s) (*(s)=='%')
#define IS_IMM(s) (*(s)=='$')

#define IS_BLANK(s) (*(s)==' ' || *(s)=='\t')
#define IS_END(s) (*(s)=='\0')

#define SKIP_BLANK(s) do {  \
  while(!IS_END(s) && IS_BLANK(s))  \
    (s)++;    \
} while(0);

/* return value from different parse_xxx function */
typedef enum { PARSE_ERR=-1, PARSE_REG, PARSE_DIGIT, PARSE_SYMBOL, 
    PARSE_MEM, PARSE_DELIM, PARSE_INSTR, PARSE_LABEL} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovl')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
    char instr[7];
    int i=0;
    /* skip the blank */
    SKIP_BLANK(*ptr);
    /* find_instr and check end */
    for(;(!IS_BLANK(*ptr))&&(!IS_END(*ptr));(*ptr)++,i++){
        instr[i]=**ptr;
    }
    /* set 'ptr' and 'inst' */
	instr[i] = '\0';
	*inst= find_instr(instr);
    if(inst==NULL)
        return PARSE_ERR;
    else 
        return PARSE_INSTR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{
    /* skip the blank and check */
	char *p = *ptr;
    SKIP_BLANK(p);
    /* set 'ptr' */
    while(*p!=delim){
        if(IS_END(p)==1)
            return PARSE_ERR;
        else
            p++;
    }
    p++;
	*ptr = p;
    return PARSE_DELIM;
}

/*
 * parse_reg: parse an expected register token (e.g., '%eax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token, 
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
    char reg[5];
    int i=0;
	char *p = *ptr;
	regid_t r = *regid;
    /* skip the blank and check */
    SKIP_BLANK(p);
    /* find register */
    for(;i<4;i++,p++)
        reg[i]=*p;
    /* set 'ptr' and 'regid' */
	reg[4] = '\0';
	*ptr = p;
    r=find_register(reg);
	*regid = r;
    if(*regid==REG_ERR)
        return PARSE_ERR;
    else 
        return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
	char *p = *ptr;
	char *pp = p;
	char *n = *name;
    int i=0;
    /* skip the blank and check */
    SKIP_BLANK(p);
	SKIP_BLANK(pp);
    /* allocate name and copy to it */
	while ((!IS_END(p))&&(!IS_BLANK(p))&&((*p)!=',')){
		i++;
		p++;
	}
    /* set 'ptr' and 'name' */
    n=(char*)malloc(i+1);
	i = 0;
	for (; (!IS_END(pp)) && (!IS_BLANK(pp)&&((*pp)!=',')); pp++, i++)
        n[i]=*pp;
	n[i] = '\0';
	*ptr = pp;
	*name = n;
    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
    long digit=0;
	char *p=*ptr;
	char ch;
	long v = *value;
	int i = 0;
    /* skip the blank and check */
    SKIP_BLANK(p);
	ch = *p;
	digit = strtoll(p, &p, 0);
    /* set 'ptr' and 'value' */
    if((*p)==ch)
        return PARSE_ERR;
     else{
        v=digit;
	*value = v;
	*ptr=p;
        return PARSE_DIGIT;
    }
}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
	char *p = *ptr;
	char *n = *name;
	long v = *value;
	parse_t temp;
    /* skip the blank and check */
    SKIP_BLANK(p);
    /* if IS_IMM, then parse the digit */
	if (IS_IMM(p) == 1){
		p++;
		temp=parse_digit(&p, &v);
		*value = v;
		return temp;
	}
    /* if IS_LETTER, then parse the symbol */
	if (IS_LETTER(p) == 1){
		temp = parse_symbol(&p, &n);
		*name = n;
		return temp;
	}
    /* set 'ptr' and 'name' or 'value' */
    else
        return PARSE_ERR;
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%ebp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
	char *p = *ptr;
	long v = *value;
	regid_t reg = *regid;
    /* skip the blank and check */
    SKIP_BLANK(p);
    /* calculate the digit and register, (ex: (%ebp) or 8(%ebp)) */
    if(parse_digit(&p,&v)==PARSE_ERR)
        v=0;
    /* set 'ptr', 'value' and 'regid' */
    if(parse_delim(&p,'(')==PARSE_ERR)
        return PARSE_ERR;
    if(parse_reg(&p,&reg)==PARSE_ERR)
        return PARSE_ERR;
    if(parse_delim(&p,')')==PARSE_ERR)
        return PARSE_ERR;
	*ptr = p;
	*value = v;
	*regid = reg;
    return PARSE_MEM;

}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
	char *p = *ptr;
	char *n = *name;
	long v = *value;
	parse_t temp;
    /* skip the blank and check */
    SKIP_BLANK(p);
    /* if IS_DIGIT, then parse the digit */
	if (IS_DIGIT(p) == 1){
		temp = parse_digit(&p, &v);
		*value = v;
                            *ptr=p;
		return temp;
	}   
    /* if IS_LETTER, then parse the symbol */
	if (IS_LETTER(p) == 1){
		temp=parse_symbol(&p, &n);
		*name = n;
        *ptr=p;
		return temp;
	}
        
    /* set 'ptr', 'name' and 'value' */
    else 
        return PARSE_ERR;
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
    int i=0;
	char *p = *ptr;
	char *pp = p;
	char *n = *name;
    /* skip the blank and check */
    SKIP_BLANK(p);
    /* allocate name and copy to it */
    while(*p!=':'){
        if(*p=='\0')
            return PARSE_INSTR;
		else {
			i++;
			p++;
		}
    }
    n=(char*)malloc(i+1);
    /* set 'ptr' and 'name' */
	i = 0;
    for(;*pp!=':';pp++,i++)
        n[i]=*pp;
	n[i] = '\0';
	if (add_symbol(n) == -1){
        err_print("Dup symbol:%s",n);
        return PARSE_ERR;
    }
	pp++;
	*ptr = pp;
	*name = n;
    return PARSE_LABEL;
}

/*
 * parse_line: parse a line of y86 code (e.g., 'Loop: mrmovl (%ecx), %esi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y86 assembly code
 *
 * return
 *     PARSE_XXX: success, fill line_t with assembled y86 code
 *     PARSE_ERR: error, try to print err information (e.g., instr type and line number)
 */
type_t parse_line(line_t *line)
{

/* when finish parse an instruction or lable, we still need to continue check 
* e.g., 
*  Loop: mrmovl (%ebp), %ecx
*           call SUM  #invoke SUM function */
    char *ptr=line->y86asm;
    char *name;
    long value=0;
    instr_t *inst=NULL;
    regid_t ra=0;
    regid_t rb=0;
    parse_t result;
	int r = 0;
    int i=0;
	line->type = TYPE_COMM;
    /* skip blank and check IS_END */
    SKIP_BLANK(ptr);
    /* is a comment ? */
    if(IS_COMMENT(ptr)==1)
        return TYPE_COMM;
    /* is a label ? */
	result = parse_label(&ptr, &name);
    if(result==PARSE_LABEL){
        line->type=TYPE_COMM;
    line->y86bin.addr=vmaddr;
    }
	if (result == PARSE_ERR)
		return TYPE_ERR;
    /* is an instruction ? */
	SKIP_BLANK(ptr);
	if (IS_END(ptr))
		return line->type;
	if (IS_COMMENT(ptr))
		return line->type;
    if(parse_instr(&ptr,&inst)==PARSE_INSTR){
        /* set type and y86bin */
        line->type=TYPE_INS;
        line->y86bin.bytes=inst->bytes;
		if (strcmp(inst->name, ".pos") && strcmp(inst->name, ".align")){
			line->y86bin.codes[0] = inst->code;
			line->y86bin.addr = vmaddr;
			/* update vmaddr */
			vmaddr += inst->bytes;
		}
    /* parse the rest of instruction according to the itype */
        for(;instr_set[i].name!=NULL;i++){
            if(!strcmp(instr_set[i].name, inst->name))
                break;
        }
        switch(i){
            case 0:case 1:case 24:
                break;
            case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 12:case 13:case 14:case 15:
                if(parse_reg(&ptr,&ra)==PARSE_ERR)
                    return TYPE_ERR;
				if (parse_delim(&ptr, ',') == PARSE_ERR){
                    err_print0("Invalid ','");
                    return TYPE_ERR;

                }
					
                if(parse_reg(&ptr,&rb)==PARSE_ERR)
                    return TYPE_ERR;
                line->y86bin.codes[1]=HPACK(ra,rb);
                break;
            case 9:
                result=parse_imm(&ptr,&name,&value);
                if(result==PARSE_ERR){
                    err_print0("Invalid Immediate");
                     return TYPE_ERR;
                }
				if (result == PARSE_DIGIT)
					memcpy(line->y86bin.codes + 2, &value, 4);
				else
					r = add_reloc(name, &(line->y86bin));
				if (parse_delim(&ptr, ',') == PARSE_ERR)
					return TYPE_ERR;
                if(parse_reg(&ptr,&rb)==PARSE_ERR)
                    return TYPE_ERR;
                line->y86bin.codes[1]=HPACK(0xF,rb);
                break;
            case 10:
                if(parse_reg(&ptr,&ra)==PARSE_ERR)
                    return TYPE_ERR;
				if (parse_delim(&ptr, ',') == PARSE_ERR)
					return TYPE_ERR;
                if(parse_mem(&ptr,&value,&rb)==PARSE_ERR)
                    return TYPE_ERR;
                line->y86bin.codes[1]=HPACK(ra,rb);
                memcpy(line->y86bin.codes+2,&value,4);
                break;
            case 11:
                if(parse_mem(&ptr,&value,&rb)==PARSE_ERR){
                    err_print0("Invalid MEM");
                    return TYPE_ERR;
                }
				if (parse_delim(&ptr, ',') == PARSE_ERR)
					return TYPE_ERR;
                if(parse_reg(&ptr,&ra)==PARSE_ERR)
                    return TYPE_ERR;
                line->y86bin.codes[1]=HPACK(ra,rb);
                memcpy(line->y86bin.codes+2,&value,4);
                break;
            case 16:case 17:case 18:case 19:case 20:case 21:case 22:case 23:
                result=parse_data(&ptr,&name,&value);
                if(result==PARSE_ERR)
                    return TYPE_ERR;
				if (result == PARSE_DIGIT){
					if (value > y86asm_lineno){
						err_print0("Invalid DEST");
						return TYPE_ERR;
					}
				
					memcpy(line->y86bin.codes + 1, &value, 4);
				}
				else
					r = add_reloc(name, &(line->y86bin));
                break;
            case 25:case 26:
                if(parse_reg(&ptr,&ra)==PARSE_ERR){
                    err_print0("Invalid REG");
                    return TYPE_ERR;
                } 
                line->y86bin.codes[1]=HPACK(ra,0xF);
                break;
            case 27:
                if(parse_digit(&ptr,&value)==PARSE_ERR)
                    return TYPE_ERR;
				
                memcpy(line->y86bin.codes,&value,1);
                break;
            case 28:
                if(parse_digit(&ptr,&value)==PARSE_ERR)
                    return TYPE_ERR;
                if(value%4==1)
                    return TYPE_ERR;
                memcpy(line->y86bin.codes,&value,2);
                break;
            case 29:
				r = parse_data(&ptr, &name,&value);
				if (r==PARSE_DIGIT)
					memcpy(line->y86bin.codes,&value,4);
				if (r == PARSE_SYMBOL)
					add_reloc(name, &(line->y86bin));
				if (r == PARSE_ERR)
					return TYPE_ERR;
                if(value%8==1)
                    return TYPE_ERR;
                break;
            case 30:
                if(parse_data(&ptr,&name,&value)==PARSE_ERR)
                    return TYPE_ERR;
				vmaddr = value;
				line->y86bin.addr = vmaddr;
                break;
            case 31:
                if(parse_digit(&ptr,&value)==PARSE_ERR)
                    return TYPE_ERR;
                while(vmaddr%value!=0)
                    vmaddr++;
                line->y86bin.addr=vmaddr;
                break;
        }
    }
    return line->type;
}

/*
 * assemble: assemble an y86 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y86 assembly file)
 *
 * return
 *     0: success, assmble the y86 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y86asm;

    /* read y86 code line-by-line, and parse them to generate raw y86 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL) {
        slen  = strlen(asm_buf);
        if ((asm_buf[slen-1] == '\n') || (asm_buf[slen-1] == '\r')) { 
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y86 assembly code */
        y86asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y86asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        /* set defualt */
        line->type = TYPE_COMM;
        line->y86asm = y86asm;
        line->next = NULL;

        /* add to y86 binary code list */
        y86bin_listtail->next = line;
        y86bin_listtail = line;
        y86asm_lineno ++;

        /* parse */
        if (parse_line(line) == TYPE_ERR)
            return -1;
    }

    /* skip line number information in err_print() */
    y86asm_lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y86 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtmp = NULL;
    symbol_t *stmp=NULL;
    rtmp = reltab->next;
	int bytes;
    while (rtmp) {
        /* find symbol */
		stmp = (symbol_t*)malloc(sizeof(symbol_t));
        stmp=find_symbol(rtmp->name);
		if (stmp == NULL){
			err_print("Unknown symbol:'%s'", rtmp->name);
			return -1;
		}
        /* relocate y86bin according itype */
        else{
			bytes = rtmp->y86bin->bytes;
            switch(bytes){
                case 6:
                    memcpy(rtmp->y86bin->codes+2,&(stmp->addr),4);
                    break;
                case 5:
                    memcpy(rtmp->y86bin->codes+1,&(stmp->addr),4);
                    break;
                case 4:
                    memcpy(rtmp->y86bin->codes,&(stmp->addr),4);
                    break;

            }
        }
        /* next */
        rtmp = rtmp->next;
    }
    return 0;
}

/*
 * binfile: generate the y86 binary file
 * args
 *     out: point to output file (an y86 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
    /* prepare image with y86 binary code */
    line_t *btmp=NULL;
    btmp=y86bin_listhead->next;
    int num=0;
    char z[]="\0";
    /* binary write y86 code to output file (NOTE: see fwrite()) */
	while (btmp != NULL){
        if(btmp->type==TYPE_INS){
                    while((num!=(btmp->y86bin.addr))&&(btmp->y86bin.bytes!=0)){
                        fwrite(z,sizeof(char),1,out);
                        num++;
                    }
                    fwrite(btmp->y86bin.codes, sizeof(char), btmp->y86bin.bytes, out);
                    num+=btmp->y86bin.bytes;
                }
	   btmp = btmp->next;
	}
            
    return 0;
}


/* whether print the readable output to screen or not ? */
bool_t screen = FALSE; 

static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        char c;
        int h = (value >> 4*i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len-i-1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[26];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS) {
        bin_t *y86bin = &line->y86bin;
        int i;
        
        strcpy(buf, "  0x000:              | ");
        
        hexstuff(buf+4, y86bin->addr, 3);
        if (y86bin->bytes > 0)
            for (i = 0; i < y86bin->bytes; i++)
                hexstuff(buf+9+2*i, y86bin->codes[i]&0xFF, 2);
    } else {
        strcpy(buf, "                      | ");
    }

    printf("%s%s\n", buf, line->y86asm);
}

/* 
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = y86bin_listhead->next;
    
    /* line by line */
    while (tmp != NULL) {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    y86bin_listhead = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(y86bin_listhead, 0, sizeof(line_t));
    y86bin_listtail = y86bin_listhead;
    y86asm_lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do {
        rtmp = reltab->next;
        if (reltab->name) 
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);
    
    symbol_t *stmp = NULL;
    do {
        stmp = symtab->next;
        if (symtab->name) 
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do {
        ltmp = y86bin_listhead->next;
        if (y86bin_listhead->y86asm) 
            free(y86bin_listhead->y86asm);
        free(y86bin_listhead);
        y86bin_listhead = ltmp;
    } while (y86bin_listhead);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;
    
    if (argc < 2)
        usage(argv[0]);
    
    if (argv[nextarg][0] == '-') {
        char flag = argv[nextarg][1];
        switch (flag) {
          case 'v':
            screen = TRUE;
            nextarg++;
            break;
          default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg])-3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg]+rootlen, ".ys"))
        usage(argv[0]);
    
    if (rootlen > 500) {
        err_print0("File name too long");
        exit(1);
    }


    /* init */
    init();

    
    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname+rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in) {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }
    
    if (assemble(in) < 0) {
        err_print0("Assemble y86 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);


    /* relocate binary code */
    if (relocate() < 0) {
        err_print0("Relocate binary code error");
        exit(1);
    }


    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname+rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out) {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0) {
        err_print0("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);
    
    /* print to screen (.yo file) */
	screen = TRUE;
    if (screen)
       print_screen(); 

    /* finit */
    finit();
    return 0;
}


