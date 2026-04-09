int converterPermissaoParaInteiro(char permissao[]){
    int codigoModificacao = 0;
    
    if (permissao[0] == 'r'){
        codigoModificacao += 4;
    }
    
    if (permissao[1] == 'w'){
        codigoModificacao += 2;
    }
    
    if (permissao[2] == 'x'){
        codigoModificacao += 1;
    }
    
    return codigoModificacao;
}

void converterPermissaoParaTexto(int permissao, char retorno[], int indiceInicial=0)
{
    if(permissao - 4 >= 0)
	{
        retorno[indiceInicial] = 'r';
        permissao -= 4;
    }
	else
        retorno[indiceInicial] = '-';
    
    if(permissao - 2 >= 0)
	{
        retorno[indiceInicial+1] = 'w';
        permissao -= 2;
    }
	else
        retorno[indiceInicial+1] = '-';
    
    if(permissao - 1 >= 0)
	{
        retorno[indiceInicial+2] = 'x';
        permissao -= 1;
    }
	else
        retorno[indiceInicial+2] = '-';

    retorno[indiceInicial+3] = '\0';
}

void converterPermissaoUGOParaTexto(int permissaoCompleta, char retorno[], int indiceInicial=0)
{
    char arrayPermissaoCompleta[4];
    itoa(permissaoCompleta, arrayPermissaoCompleta, 10);

    int permissao[3] = {
        arrayPermissaoCompleta[0] - '0',
        arrayPermissaoCompleta[1] - '0',
        arrayPermissaoCompleta[2] - '0'
    };

    converterPermissaoParaTexto(permissao[0], retorno, indiceInicial);
    converterPermissaoParaTexto(permissao[1], retorno, indiceInicial+3);
    converterPermissaoParaTexto(permissao[2], retorno, indiceInicial+6);

    retorno[indiceInicial+9] = '\0';
}

int converterPermissaoUGOParaInteiro(char permissaoCompleta[], int indiceInicial=0)
{
    int permissao = 0;
    char arrayPermissaoIndividual[3];

    //usuario (7xx)
    strncpy(arrayPermissaoIndividual, permissaoCompleta+indiceInicial, 3);
    permissao += converterPermissaoParaInteiro(arrayPermissaoIndividual) * 100;

    //grupo (x7x)
    strncpy(arrayPermissaoIndividual, permissaoCompleta+indiceInicial+3, 3);
    permissao += converterPermissaoParaInteiro(arrayPermissaoIndividual) * 10;

    //outros (xx7)
    strncpy(arrayPermissaoIndividual, permissaoCompleta+indiceInicial+6, 3);
    permissao += converterPermissaoParaInteiro(arrayPermissaoIndividual);

    return permissao;
}
