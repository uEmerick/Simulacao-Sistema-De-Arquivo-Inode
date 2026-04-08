// Converte uma string de permissão (rwx) para um inteiro.
int permission_to_int(char permissao[]){
    int chmodCod = 0;
    
    if (permissao[0] == 'r'){
        chmodCod += 4;
    }
    
    if (permissao[1] == 'w'){
        chmodCod += 2;
    }
    
    if (permissao[2] == 'x'){
        chmodCod += 1;
    }
    
    return chmodCod;
}

// Converte um inteiro de permissão para uma string (rwx).
void convert_permission_to_string(int permissao, char retorno[], int posicaoInicial=0)
{
    if(permissao - 4 >= 0)
	{
        retorno[posicaoInicial] = 'r';
        permissao -= 4;
    }
	else
        retorno[posicaoInicial] = '-';
    
    if(permissao - 2 >= 0)
	{
        retorno[posicaoInicial+1] = 'w';
        permissao -= 2;
    }
	else
        retorno[posicaoInicial+1] = '-';
    
    if(permissao - 1 >= 0)
	{
        retorno[posicaoInicial+2] = 'x';
        permissao -= 1;
    }
	else
        retorno[posicaoInicial+2] = '-';

    retorno[posicaoInicial+3] = '\0';
}

// Converte um inteiro de permissão UGO (User, Group, Others) para uma string (rwxrwxrwx).
void convert_permission_ugo_to_string(int permissaoCompleta, char retorno[], int posicaoInicial=0)
{
    char permissaoCompletaChar[4];
    itoa(permissaoCompleta, permissaoCompletaChar, 10);

    int perm[3] = {
        permissaoCompletaChar[0] - '0',
        permissaoCompletaChar[1] - '0',
        permissaoCompletaChar[2] - '0'
    };

    convert_permission_to_string(perm[0], retorno, posicaoInicial);
    convert_permission_to_string(perm[1], retorno, posicaoInicial+3);
    convert_permission_to_string(perm[2], retorno, posicaoInicial+6);

    retorno[posicaoInicial+9] = '\0';
}

// Converte uma string de permissão UGO (rwxrwxrwx) para um inteiro.
int convert_permission_ugo_to_int(char permissaoCompleta[], int posicaoInicial=0)
{
    int permissao = 0;
    char permissaoIndividual[3];

    //usuario (7xx)
    strncpy(permissaoIndividual, permissaoCompleta+posicaoInicial, 3);
    permissao += permission_to_int(permissaoIndividual) * 100;

    //grupo (x7x)
    strncpy(permissaoIndividual, permissaoCompleta+posicaoInicial+3, 3);
    permissao += permission_to_int(permissaoIndividual) * 10;

    //outros (xx7)
    strncpy(permissaoIndividual, permissaoCompleta+posicaoInicial+6, 3);
    permissao += permission_to_int(permissaoIndividual);

    return permissao;
}
