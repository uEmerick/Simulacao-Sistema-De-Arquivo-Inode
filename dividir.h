#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

vector<string> dividirString(string textoOriginal, char separador=' ')
{
    istringstream fluxoTexto(textoOriginal);
    string textoTemporario;

    vector<string> resultado;

    while(getline(fluxoTexto, textoTemporario, separador))
    {
        resultado.push_back(textoTemporario);
    }

    return resultado;
}

vector<string> dividirCaminho(string textoOriginal)
{
    vector<string> caminhoDecomposicao;

    if(textoOriginal.at(0) == '/') {
        caminhoDecomposicao.push_back("/");

        for(const string& elemento : dividirString(textoOriginal, '/')) 
        {
            if (strcmp(elemento.c_str(), "") != 0)
                caminhoDecomposicao.push_back(elemento);
        }
    }
    else 
	{
        caminhoDecomposicao = dividirString(textoOriginal, '/');
    }

    return caminhoDecomposicao;
}

string obterUltimaPosicao(vector<string> vetor)
{
    return vetor.at(vetor.size()-1);
}

int contarOcorrenciasNaString(string conteudo, char caracterBuscado)
{
    int quantidade=0;
    for(int i=0; i<conteudo.size(); i++)
    {
        if(caracterBuscado == conteudo.at(i))
            quantidade++;
    }

    return quantidade;
}

string converterParaMinusculas(string palavra)
{
    for(auto& caractere : palavra) 
	{ 
        caractere = tolower(caractere); 
    }
    return palavra;
}
