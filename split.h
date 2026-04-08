#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

//Divide uma string em um vetor de strings, usando um delimitador.
vector<string> split_string(string stringPrincipal, char delimitador=' ')
{
    istringstream stringStream(stringPrincipal);
    string stringAuxiliar;

    vector<string> retorno;

    while(getline(stringStream, stringAuxiliar, delimitador))
    {
        retorno.push_back(stringAuxiliar);
    }

    return retorno;
}

// Divide um caminho de arquivo em um vetor de diretórios/arquivos.
vector<string> split_file_path(string stringPrincipal)
{
    vector<string> caminhoOrigem;

    if(stringPrincipal.at(0) == '/') {
        caminhoOrigem.push_back("/");

        for(const string& elem : split_string(stringPrincipal, '/'))
        {
            if (strcmp(elem.c_str(), "") != 0)
                caminhoOrigem.push_back(elem);
        }
    }
    else
	{
        caminhoOrigem = split_string(stringPrincipal, '/');
    }

    return caminhoOrigem;
}

// Retorna o último elemento de um vetor de strings.
string get_last_element(vector<string> vector)
{
    return vector.at(vector.size()-1);
}

// Conta o número de ocorrências de um caractere em uma string.
int count_occurrences(string conteudo, char caracterBuscado)
{
    int qtd=0;
    for(int i=0; i<conteudo.size(); i++)
    {
        if(caracterBuscado == conteudo.at(i))
            qtd++;
    }

    return qtd;
}

//Converte uma string para minúsculas.
string string_to_lower_case(string palavra)
{
    for(auto& x : palavra) 
	{ 
        x = tolower(x); 
    }
    return palavra;
}
