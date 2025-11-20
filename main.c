/*abaixo estão as bibliotecas usadas*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*Estava tendo dificuldades em fazer um código onde não tinha limite
 *de usuários ou livros, então decidi limitar a um valor.*/
#define MAX 10000
#define MAX_LINE 1024

/* ---- Tipos ---- */
typedef struct {
    int dia;
    int mes;
    int ano;
} Data;

typedef struct {
    int matricula;               /* inteiro */
    char nome[101];              /* ate 100 chars + \\0 */
    char curso[51];              /* ate 50 chars */
    char telefone[16];           /* ate 15 chars */
    Data dataCadastro;
} Usuario;

typedef struct {
    int codigo;                  /* inteiro */
    char titulo[101];            /* 100 */
    char autor[81];              /* 80 */
    char editora[61];            /* 60 */
    int anoPublicacao;           /* inteiro */
    int exemplares;              /* numero de exemplares disponiveis (total atual) */
    int totalInitial;            /* numero total inicialmente cadastrado (para controle) */
    int totalEmprestado;         /* contador para relatorio "mais emprestado" */
} Livro;

typedef struct {
    int codigoEmprestimo;        /* inteiro */
    int matriculaUsuario;
    int codigoLivro;
    Data dataEmprestimo;
    Data dataPrevista;           /* 7 dias apos */
    int status;                  /* 0 = ativo, 1 = devolvido */
    int renovacoes;              /* numero de renovacoes */
} Emprestimo;

/* ---- Bancos em memoria (arrays circulares) ---- */
Usuario usuarios[MAX];
Livro livros[MAX];
Emprestimo emprestimos[MAX];

int countUsuarios = 0;        /* numero atual de registros (max MAX) */
int idxUsuariosNext = 0;      /* proximo indice para inserir (circular) */

int countLivros = 0;
int idxLivrosNext = 0;

int countEmprestimos = 0;
int idxEmprestimosNext = 0;

/* codigo sequencial de emprestimo (para gerar codigo unico crescente) */
int nextCodigoEmprestimo = 1;

/* ---- Utilitarios ---- */
void trim_newline(char *s) {
    if (!s) return;
    size_t L = strlen(s);
    if (L == 0) return;
    if (s[L-1] == '\n') s[L-1] = '\0';
}

/* data hoje */
Data data_hoje() {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    Data d;
    d.dia = tm.tm_mday;
    d.mes = tm.tm_mon + 1;
    d.ano = tm.tm_year + 1900;
    return d;
}

/* compara datas: >0 se a>b, 0 se iguais, <0 se a<b */
int comparar_datas(Data a, Data b) {
    if (a.ano != b.ano) return a.ano - b.ano;
    if (a.mes != b.mes) return a.mes - b.mes;
    return a.dia - b.dia;
}

/* adiciona dias corretamente usando mktime */
Data adicionar_dias(Data d, int dias) {
    struct tm tm = {0};
    tm.tm_mday = d.dia + dias;
    tm.tm_mon  = d.mes - 1;
    tm.tm_year = d.ano - 1900;
    mktime(&tm); /* normaliza */
    Data r;
    r.dia = tm.tm_mday; r.mes = tm.tm_mon + 1; r.ano = tm.tm_year + 1900;
    return r;
}

/* ano mes dia _ hora minuto segundo */
void timestamp(char *buf, size_t sz) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(buf, sz, "%Y%m%d_%H%M%S", &tm);
}

/* copia arquivo src -> dest (retorna 0 sucesso) */
int copiar_arquivo(const char *src, const char *dest) {
    FILE *fsrc = fopen(src, "rb");
    if (!fsrc) return -1;
    FILE *fdst = fopen(dest, "wb");
    if (!fdst) { fclose(fsrc); return -2; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        fwrite(buf, 1, n, fdst);
    }
    fclose(fsrc); fclose(fdst);
    return 0;
}

/* backup automatico de um arquivo se existir */
void backup_arquivo_se_existe(const char *nome) {
    FILE *f = fopen(nome, "rb");
    if (!f) return;
    fclose(f);
    char ts[32];
    timestamp(ts, sizeof(ts));
    char dest[256];
    snprintf(dest, sizeof(dest), "backup_%s_%s", ts, nome);
    copiar_arquivo(nome, dest);
}

/* backup de todos os arquivos */
void backup_tudo() {
    backup_arquivo_se_existe("usuarios.txt");
    backup_arquivo_se_existe("livros.txt");
    backup_arquivo_se_existe("emprestimos.txt");
}

/* ---- Buscas auxiliares ---- */
int buscar_usuario_por_matricula(int matricula) {
    for (int i = 0; i < countUsuarios; ++i) {
        /* banco pode ter insercoes sobrescrevendo indices, entao percorremos linear */
        if (usuarios[i].matricula == matricula) return i;
    }
    return -1;
}

int buscar_livro_por_codigo(int codigo) {
    for (int i = 0; i < countLivros; ++i) {
        if (livros[i].codigo == codigo) return i;
    }
    return -1;
}

Emprestimo* buscar_emprestimo_por_codigo_ptr(int codigo) {
    for (int i = 0; i < countEmprestimos; ++i) {
        if (emprestimos[i].codigoEmprestimo == codigo) return &emprestimos[i];
    }
    return NULL;
}

/* ---- Salvamento / Carregamento ---- */

/* formados simples com '|' como separador e uma linha por registro */
void salvar_usuarios() {
    backup_arquivo_se_existe("usuarios.txt");
    FILE *f = fopen("usuarios.txt", "w");
    if (!f) { printf("Erro ao salvar usuarios.txt\n"); return; }
    for (int i = 0; i < countUsuarios; ++i) {
        Usuario *u = &usuarios[i];
        fprintf(f, "%d|%s|%s|%s|%d-%d-%d\n",
            u->matricula, u->nome, u->curso, u->telefone,
            u->dataCadastro.dia, u->dataCadastro.mes, u->dataCadastro.ano);
    }
    fclose(f);
}

void salvar_livros() {
    backup_arquivo_se_existe("livros.txt");
    FILE *f = fopen("livros.txt", "w");
    if (!f) { printf("Erro ao salvar livros.txt\n"); return; }
    for (int i = 0; i < countLivros; ++i) {
        Livro *l = &livros[i];
        fprintf(f, "%d|%s|%s|%s|%d|%d|%d\n",
            l->codigo, l->titulo, l->autor, l->editora,
            l->anoPublicacao, l->exemplares, l->totalEmprestado);
    }
    fclose(f);
}

void salvar_emprestimos() {
    backup_arquivo_se_existe("emprestimos.txt");
    FILE *f = fopen("emprestimos.txt", "w");
    if (!f) { printf("Erro ao salvar emprestimos.txt\n"); return; }
    for (int i = 0; i < countEmprestimos; ++i) {
        Emprestimo *e = &emprestimos[i];
        fprintf(f, "%d|%d|%d|%d-%d-%d|%d-%d-%d|%d|%d\n",
            e->codigoEmprestimo, e->matriculaUsuario, e->codigoLivro,
            e->dataEmprestimo.dia, e->dataEmprestimo.mes, e->dataEmprestimo.ano,
            e->dataPrevista.dia, e->dataPrevista.mes, e->dataPrevista.ano,
            e->renovacoes, e->status);
    }
    fclose(f);
}

void salvar_tudo() {
    salvar_usuarios();
    salvar_livros();
    salvar_emprestimos();
    /* backup ja foi feito em cada salvar, mas podemos fazer geral */
    /* backup_tudo(); */
}

/* Carregar: se arquivo ausente, ignora */
void carregar_usuarios() {
    FILE *f = fopen("usuarios.txt", "r");
    if (!f) return;
    char linha[MAX_LINE];
    countUsuarios = 0;
    while (fgets(linha, sizeof(linha), f)) {
        trim_newline(linha);
        if (strlen(linha) == 0) continue;
        Usuario u;
        char *tok = strtok(linha, "|"); if (!tok) continue; u.matricula = atoi(tok);
        tok = strtok(NULL, "|"); if (!tok) continue; strncpy(u.nome, tok, sizeof(u.nome)-1); u.nome[100-1]=0;
        tok = strtok(NULL, "|"); if (!tok) continue; strncpy(u.curso, tok, sizeof(u.curso)-1); u.curso[50-1]=0;
        tok = strtok(NULL, "|"); if (!tok) continue; strncpy(u.telefone, tok, sizeof(u.telefone)-1); u.telefone[16-1]=0;
        tok = strtok(NULL, "|");
        if (tok) {
            int d,m,a;
            if (sscanf(tok, "%d-%d-%d", &d,&m,&a) == 3) { u.dataCadastro.dia=d; u.dataCadastro.mes=m; u.dataCadastro.ano=a; }
        } else {
            u.dataCadastro = data_hoje();
        }
        usuarios[countUsuarios++] = u;
        if (countUsuarios >= MAX) break;
    }
    fclose(f);
}

void carregar_livros() {
    FILE *f = fopen("livros.txt", "r");
    if (!f) return;
    char linha[MAX_LINE];
    countLivros = 0;
    while (fgets(linha, sizeof(linha), f)) {
        trim_newline(linha);
        if (strlen(linha) == 0) continue;
        Livro l;
        char *tok = strtok(linha, "|"); if(!tok) continue; l.codigo = atoi(tok);
        tok = strtok(NULL, "|"); if(!tok) continue; strncpy(l.titulo, tok, sizeof(l.titulo)-1); l.titulo[100-1]=0;
        tok = strtok(NULL, "|"); if(!tok) continue; strncpy(l.autor, tok, sizeof(l.autor)-1); l.autor[80-1]=0;
        tok = strtok(NULL, "|"); if(!tok) continue; strncpy(l.editora, tok, sizeof(l.editora)-1); l.editora[60-1]=0;
        tok = strtok(NULL, "|"); if(!tok) continue; l.anoPublicacao = atoi(tok);
        tok = strtok(NULL, "|"); if(!tok) continue; l.exemplares = atoi(tok);
        tok = strtok(NULL, "|"); if(!tok) continue; l.totalEmprestado = atoi(tok);
        l.totalInitial = l.exemplares;
        if (countLivros < MAX) livros[countLivros++] = l;
    }
    fclose(f);
}

void carregar_emprestimos() {
    FILE *f = fopen("emprestimos.txt", "r");
    if (!f) return;
    char linha[MAX_LINE];
    countEmprestimos = 0;
    int maiorCodigo = 0;
    while (fgets(linha, sizeof(linha), f)) {
        trim_newline(linha);
        if (strlen(linha) == 0) continue;
        Emprestimo e;
        char *tok = strtok(linha, "|"); if(!tok) continue; e.codigoEmprestimo = atoi(tok);
        tok = strtok(NULL, "|"); if(!tok) continue; e.matriculaUsuario = atoi(tok);
        tok = strtok(NULL, "|"); if(!tok) continue; e.codigoLivro = atoi(tok);
        tok = strtok(NULL, "|"); if(tok) { int d,m,a; if (sscanf(tok, "%d-%d-%d",&d,&m,&a)==3) { e.dataEmprestimo.dia=d; e.dataEmprestimo.mes=m; e.dataEmprestimo.ano=a; } }
        tok = strtok(NULL, "|"); if(tok) { int d,m,a; if (sscanf(tok, "%d-%d-%d",&d,&m,&a)==3) { e.dataPrevista.dia=d; e.dataPrevista.mes=m; e.dataPrevista.ano=a; } }
        tok = strtok(NULL, "|"); if(!tok) continue; e.renovacoes = atoi(tok);
        tok = strtok(NULL, "|"); if(!tok) continue; e.status = atoi(tok);
        if (countEmprestimos < MAX) emprestimos[countEmprestimos++] = e;
        if (e.codigoEmprestimo > maiorCodigo) maiorCodigo = e.codigoEmprestimo;
    }
    fclose(f);
    nextCodigoEmprestimo = (maiorCodigo >= 1) ? (maiorCodigo + 1) : 1;
}

/* carrega tudo */
void carregar_tudo() {
    carregar_usuarios();
    carregar_livros();
    carregar_emprestimos();
}

/* ---- Funcoes de cadastro ---- */

void cadastrar_usuario() {
    Usuario u;
    printf("Matricula (inteiro): ");
    if (scanf("%d", &u.matricula) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
    getchar();
    printf("Nome completo: "); fgets(u.nome, sizeof(u.nome), stdin); trim_newline(u.nome);
    printf("Curso: "); fgets(u.curso, sizeof(u.curso), stdin); trim_newline(u.curso);
    printf("Telefone: "); fgets(u.telefone, sizeof(u.telefone), stdin); trim_newline(u.telefone);
    u.dataCadastro = data_hoje();

    /* evitar duplicata de matricula */
    if (buscar_usuario_por_matricula(u.matricula) != -1) {
        printf("Matricula ja cadastrada. Cancelando.\n");
        return;
    }

    usuarios[idxUsuariosNext] = u;
    idxUsuariosNext = (idxUsuariosNext + 1) % MAX;
    if (countUsuarios < MAX) countUsuarios++;

    salvar_usuarios();
    backup_tudo();

    printf("Usuario cadastrado com sucesso.\n");
}

void cadastrar_livro() {
    Livro l;
    printf("Codigo do livro (inteiro): ");
    if (scanf("%d", &l.codigo) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
    getchar();
    printf("Titulo: "); fgets(l.titulo, sizeof(l.titulo), stdin); trim_newline(l.titulo);
    printf("Autor: "); fgets(l.autor, sizeof(l.autor), stdin); trim_newline(l.autor);
    printf("Editora: "); fgets(l.editora, sizeof(l.editora), stdin); trim_newline(l.editora);
    printf("Ano de publicacao: "); if (scanf("%d", &l.anoPublicacao) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
    printf("Numero de exemplares: "); if (scanf("%d", &l.exemplares) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
    getchar();
    l.totalInitial = l.exemplares;
    l.totalEmprestado = 0;

    /* checar duplicata de codigo */
    if (buscar_livro_por_codigo(l.codigo) != -1) {
        printf("Codigo ja cadastrado. Cancelando.\n");
        return;
    }

    livros[idxLivrosNext] = l;
    idxLivrosNext = (idxLivrosNext + 1) % MAX;
    if (countLivros < MAX) countLivros++;

    salvar_livros();
    backup_tudo();

    printf("Livro cadastrado com sucesso.\n");
}

/* ---- Funcoes de pesquisa ---- */

/* pesquisar livros por codigo, titulo ou autor */
void pesquisar_livros() {
    int opc;
    printf("Pesquisar livros por: 1-codigo 2-titulo 3-autor\nOpcao: ");
    if (scanf("%d", &opc) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
    getchar();
    if (opc == 1) {
        int cod; printf("Codigo: "); if (scanf("%d", &cod) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
        int idx = buscar_livro_por_codigo(cod);
        if (idx == -1) { printf("Livro nao encontrado.\n"); return; }
        Livro *l = &livros[idx];
        printf("Codigo:%d\nTitulo:%s\nAutor:%s\nEditora:%s\nAno:%d\nExemplares disponiveis:%d\nTotal emprestado:%d\n",
            l->codigo, l->titulo, l->autor, l->editora, l->anoPublicacao, l->exemplares, l->totalEmprestado);
    } else if (opc == 2 || opc == 3) {
        char termo[256];
        if (opc == 2) printf("Titulo (substring): ");
        else printf("Autor (substring): ");
        fgets(termo, sizeof(termo), stdin); trim_newline(termo);
        int encontrados = 0;
        for (int i = 0; i < countLivros; ++i) {
            if ((opc==2 && strstr(livros[i].titulo, termo)) || (opc==3 && strstr(livros[i].autor, termo))) {
                printf("Codigo:%d | Titulo:%s | Autor:%s | Disponiveis:%d | TotalEmp:%d\n",
                    livros[i].codigo, livros[i].titulo, livros[i].autor,
                    livros[i].exemplares - livros[i].totalEmprestado + livros[i].totalEmprestado /* simplified display */,
                    livros[i].totalEmprestado);
                encontrados++;
            }
        }
        if (!encontrados) printf("Nenhum livro encontrado.\n");
    } else {
        printf("Opcao invalida.\n");
    }
}

/* pesquisar usuarios por matricula ou nome */
void pesquisar_usuarios() {
    int opc;
    printf("Pesquisar usuarios por: 1-matricula 2-nome\nOpcao: ");
    if (scanf("%d", &opc) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
    getchar();
    if (opc == 1) {
        int mat; printf("Matricula: ");
        if (scanf("%d", &mat) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
        int idx = buscar_usuario_por_matricula(mat);
        if (idx == -1) { printf("Usuario nao encontrado.\n"); return; }
        Usuario *u = &usuarios[idx];
        printf("Matricula:%d\nNome:%s\nCurso:%s\nTelefone:%s\nData Cadastro:%02d/%02d/%04d\n",
            u->matricula, u->nome, u->curso, u->telefone, u->dataCadastro.dia, u->dataCadastro.mes, u->dataCadastro.ano);
    } else if (opc == 2) {
        char termo[256]; printf("Nome (substring): ");
        fgets(termo, sizeof(termo), stdin); trim_newline(termo);
        int encontrados = 0;
        for (int i = 0; i < countUsuarios; ++i) {
            if (strstr(usuarios[i].nome, termo)) {
                Usuario *u = &usuarios[i];
                printf("Mat:%d | Nome:%s | Curso:%s | Telefone:%s\n",
                    u->matricula, u->nome, u->curso, u->telefone);
                encontrados++;
            }
        }
        if (!encontrados) printf("Nenhum usuario encontrado.\n");
    } else {
        printf("Opcao invalida.\n");
    }
}

/* ---- Emprestimos: realizar, devolver, renovar, listar ativos ---- */

void realizar_emprestimo() {
    if (countEmprestimos >= MAX) { /* sobrescrever circular */
        /* idxEmprestimosNext ja controla posicao */
    }
    Emprestimo e;
    printf("Matricula do usuario: ");
    if (scanf("%d", &e.matriculaUsuario) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
    int idxU = buscar_usuario_por_matricula(e.matriculaUsuario);
    if (idxU == -1) { printf("Usuario nao cadastrado.\n"); return; }

    printf("Codigo do livro: ");
    if (scanf("%d", &e.codigoLivro) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }
    int idxL = buscar_livro_por_codigo(e.codigoLivro);
    if (idxL == -1) { printf("Livro nao encontrado.\n"); return; }

    if (livros[idxL].exemplares <= 0) {
        printf("Nao ha exemplares disponiveis deste livro.\n");
        return;
    }

    e.codigoEmprestimo = nextCodigoEmprestimo++;
    e.dataEmprestimo = data_hoje();
    e.dataPrevista = adicionar_dias(e.dataEmprestimo, 7);
    e.status = 0; /* 0 = ativo, 1 = devolvido -> here using 0 active, 1 devolvido */
    e.renovacoes = 0;

    /* inserir circular */
    emprestimos[idxEmprestimosNext] = e;
    idxEmprestimosNext = (idxEmprestimosNext + 1) % MAX;
    if (countEmprestimos < MAX) countEmprestimos++;

    /* atualizar livro: diminuir exemplares e incrementar totalEmprestado */
    livros[idxL].exemplares -= 1;
    livros[idxL].totalEmprestado += 1;

    salvar_livros();
    salvar_emprestimos();
    backup_tudo();

    printf("Emprestimo realizado. Codigo emprestimo: %d\n", e.codigoEmprestimo);
    printf("Data prevista de devolucao: %02d/%02d/%04d\n", e.dataPrevista.dia, e.dataPrevista.mes, e.dataPrevista.ano);
}

void realizar_devolucao() {
    int cod;
    printf("Codigo do emprestimo: ");
    if (scanf("%d", &cod) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }

    for (int i = 0; i < countEmprestimos; ++i) {
        if (emprestimos[i].codigoEmprestimo == cod && emprestimos[i].status == 0) {
            /* marcar devolvido */
            emprestimos[i].status = 1;
            /* aumentar exemplares no livro */
            int idxL = buscar_livro_por_codigo(emprestimos[i].codigoLivro);
            if (idxL != -1) {
                livros[idxL].exemplares += 1;
            }
            salvar_livros();
            salvar_emprestimos();
            backup_tudo();
            printf("Devolucao registrada com sucesso.\n");
            return;
        }
    }
    printf("Emprestimo ativo nao encontrado com esse codigo.\n");
}

void renovar_emprestimo() {
    int cod;
    printf("Codigo do emprestimo: ");
    if (scanf("%d", &cod) != 1) { while(getchar()!='\n'); printf("Entrada invalida.\n"); return; }

    for (int i = 0; i < countEmprestimos; ++i) {
        if (emprestimos[i].codigoEmprestimo == cod && emprestimos[i].status == 0) {
            if (emprestimos[i].renovacoes >= 2) { printf("Limite de renovacoes atingido.\n"); return; }
            emprestimos[i].renovacoes += 1;
            emprestimos[i].dataPrevista = adicionar_dias(emprestimos[i].dataPrevista, 7);
            salvar_emprestimos();
            backup_tudo();
            printf("Emprestimo renovado. Nova data prevista: %02d/%02d/%04d\n",
                emprestimos[i].dataPrevista.dia, emprestimos[i].dataPrevista.mes, emprestimos[i].dataPrevista.ano);
            return;
        }
    }
    printf("Emprestimo ativo nao encontrado.\n");
}

void listar_emprestimos_ativos() {
    int achou = 0;
    Data hojeData = data_hoje();
    printf("Emprestimos ativos:\n");
    for (int i = 0; i < countEmprestimos; ++i) {
        Emprestimo *e = &emprestimos[i];
        if (e->status == 0) {
            printf("CodigoEmp:%d | Matricula:%d | Livro:%d | Emprestimo:%02d/%02d/%04d | Prev:%02d/%02d/%04d | Renov:%d\n",
                e->codigoEmprestimo, e->matriculaUsuario, e->codigoLivro,
                e->dataEmprestimo.dia, e->dataEmprestimo.mes, e->dataEmprestimo.ano,
                e->dataPrevista.dia, e->dataPrevista.mes, e->dataPrevista.ano,
                e->renovacoes);
            achou = 1;
        }
    }
    if (!achou) printf("Nenhum emprestimo ativo.\n");
}

/* ---- Relatorios e busca avancada ---- */

/* top N livros mais emprestados (usa totalEmprestado) */
void relatorio_livros_mais_emprestados() {
    if (countLivros == 0) { printf("Nenhum livro cadastrado.\n"); return; }
    int N = 10;
    int n = (countLivros < N) ? countLivros : N;
    /* copiar indices */
    int idxs[countLivros];
    for (int i = 0; i < countLivros; ++i) idxs[i] = i;
    /* sort decrescente por totalEmprestado (selection sort simplificado) */
    for (int i = 0; i < countLivros - 1; ++i) {
        for (int j = i + 1; j < countLivros; ++j) {
            if (livros[idxs[j]].totalEmprestado > livros[idxs[i]].totalEmprestado) {
                int t = idxs[i]; idxs[i] = idxs[j]; idxs[j] = t;
            }
        }
    }
    printf("Top %d livros mais emprestados:\n", n);
    for (int i = 0; i < n; ++i) {
        Livro *l = &livros[idxs[i]];
        printf("%d) Codigo:%d | Titulo:%s | Autor:%s | TotalEmprestado:%d\n",
            i+1, l->codigo, l->titulo, l->autor, l->totalEmprestado);
    }
}

/* usuarios com emprestimos em atraso */
void relatorio_usuarios_em_atraso() {
    Data hojeData = data_hoje();
    int encontrou = 0;
    for (int i = 0; i < countEmprestimos; ++i) {
        Emprestimo *e = &emprestimos[i];
        if (e->status == 0 && comparar_datas(e->dataPrevista, hojeData) < 0) {
            int idxU = buscar_usuario_por_matricula(e->matriculaUsuario);
            Usuario *u = (idxU != -1) ? &usuarios[idxU] : NULL;
            int idxL = buscar_livro_por_codigo(e->codigoLivro);
            Livro *l = (idxL != -1) ? &livros[idxL] : NULL;
            printf("Emp:%d | Mat:%d | Nome:%s | Livro:%d | Titulo:%s | Prev:%02d/%02d/%04d\n",
                e->codigoEmprestimo, e->matriculaUsuario, u ? u->nome : "N/C",
                e->codigoLivro, l ? l->titulo : "N/C",
                e->dataPrevista.dia, e->dataPrevista.mes, e->dataPrevista.ano);
            encontrou = 1;
        }
    }
    if (!encontrou) printf("Nenhum emprestimo em atraso.\n");
}

/* busca avancada: titulo, autor, editora, ano range (ano Minimo/ano Maximo 0 para pular) */
void busca_avancada() {
    char titulo[256] = "", autor[256] = "", editora[256] = "";
    char s[64];
    int anoMin = 0, anoMax = 0;
    printf("Busca avancada - preencha campos (enter para pular)\n");
    printf("Titulo (substring): "); fgets(titulo, sizeof(titulo), stdin); trim_newline(titulo);
    printf("Autor (substring): "); fgets(autor, sizeof(autor), stdin); trim_newline(autor);
    printf("Editora (substring): "); fgets(editora, sizeof(editora), stdin); trim_newline(editora);
    printf("Ano minimo (0 para pular): "); fgets(s, sizeof(s), stdin); anoMin = atoi(s);
    printf("Ano maximo (0 para pular): "); fgets(s, sizeof(s), stdin); anoMax = atoi(s);
    int achou = 0;
    for (int i = 0; i < countLivros; ++i) {
        Livro *l = &livros[i];
        int ok = 1;
        if (strlen(titulo) && !strstr(l->titulo, titulo)) ok = 0;
        if (strlen(autor) && !strstr(l->autor, autor)) ok = 0;
        if (strlen(editora) && !strstr(l->editora, editora)) ok = 0;
        if (anoMin != 0 && l->anoPublicacao < anoMin) ok = 0;
        if (anoMax != 0 && l->anoPublicacao > anoMax) ok = 0;
        if (ok) {
            printf("Codigo:%d | Titulo:%s | Autor:%s | Editora:%s | Ano:%d | Exemplares:%d\n",
                l->codigo, l->titulo, l->autor, l->editora, l->anoPublicacao, l->exemplares);
            achou = 1;
        }
    }
    if (!achou) printf("Nenhum resultado.\n");
}

/* ---- Menus e submenus ---- */

void menu_usuarios() {
    int op;
    do {
        printf("\n-- Menu Usuarios --\n");
        printf("1 - Cadastrar usuario\n");
        printf("2 - Pesquisar usuario\n");
        printf("3 - Listar usuarios\n");
        printf("0 - Voltar\n");
        printf("Opcao: "); if (scanf("%d", &op) != 1) { while(getchar()!='\n'); op = -1; }
        getchar();
        if (op == 1) cadastrar_usuario();
        else if (op == 2) pesquisar_usuarios();
        else if (op == 3) {
            if (countUsuarios == 0) printf("Nenhum usuario cadastrado.\n");
            for (int i = 0; i < countUsuarios; ++i) {
                Usuario *u = &usuarios[i];
                printf("Mat:%d | Nome:%s | Curso:%s | Tel:%s | Cadastro:%02d/%02d/%04d\n",
                    u->matricula, u->nome, u->curso, u->telefone, u->dataCadastro.dia, u->dataCadastro.mes, u->dataCadastro.ano);
            }
        }
    } while (op != 0);
}

void menu_livros() {
    int op;
    do {
        printf("\n-- Menu Livros --\n");
        printf("1 - Cadastrar livro\n");
        printf("2 - Pesquisar livro\n");
        printf("3 - Listar todos livros\n");
        printf("4 - Busca avancada\n");
        printf("0 - Voltar\n");
        printf("Opcao: "); if (scanf("%d", &op) != 1) { while(getchar()!='\n'); op = -1; }
        getchar();
        if (op == 1) cadastrar_livro();
        else if (op == 2) pesquisar_livros();
        else if (op == 3) {
            if (countLivros == 0) printf("Nenhum livro cadastrado.\n");
            for (int i = 0; i < countLivros; ++i) {
                Livro *l = &livros[i];
                printf("Codigo:%d | Titulo:%s | Autor:%s | Editora:%s | Ano:%d | Exemplares:%d | TotalEmp:%d\n",
                    l->codigo, l->titulo, l->autor, l->editora, l->anoPublicacao, l->exemplares, l->totalEmprestado);
            }
        }
        else if (op == 4) busca_avancada();
    } while (op != 0);
}

void menu_emprestimos() {
    int op;
    do {
        printf("\n-- Menu Emprestimos --\n");
        printf("1 - Realizar emprestimo\n");
        printf("2 - Devolver\n");
        printf("3 - Renovar emprestimo\n");
        printf("4 - Listar emprestimos ativos\n");
        printf("0 - Voltar\n");
        printf("Opcao: "); if (scanf("%d", &op) != 1) { while(getchar()!='\n'); op = -1; }
        getchar();
        if (op == 1) realizar_emprestimo();
        else if (op == 2) realizar_devolucao();
        else if (op == 3) renovar_emprestimo();
        else if (op == 4) listar_emprestimos_ativos();
    } while (op != 0);
}

void menu_relatorios() {
    int op;
    do {
        printf("\n-- Menu Relatorios --\n");
        printf("1 - Livros mais emprestados\n");
        printf("2 - Usuarios com emprestimos em atraso\n");
        printf("0 - Voltar\n");
        printf("Opcao: "); if (scanf("%d", &op) != 1) { while(getchar()!='\n'); op = -1; }
        getchar();
        if (op == 1) relatorio_livros_mais_emprestados();
        else if (op == 2) relatorio_usuarios_em_atraso();
    } while (op != 0);
}

/* menu principal simples */
void menu_principal() {
    int op;
    do {
        printf("\n=== SISTEMA DE BIBLIOTECA ===\n");
        printf("1 - Usuarios\n");
        printf("2 - Livros\n");
        printf("3 - Emprestimos\n");
        printf("4 - Relatorios\n");
        printf("5 - Salvar dados\n");
        printf("6 - Carregar dados\n");
        printf("0 - Sair\n");
        printf("Opcao: "); if (scanf("%d", &op) != 1) { while(getchar()!='\n'); op = -1; }
        getchar();
        if (op == 1) menu_usuarios();
        else if (op == 2) menu_livros();
        else if (op == 3) menu_emprestimos();
        else if (op == 4) menu_relatorios();
        else if (op == 5) { salvar_tudo(); printf("Dados salvos.\n"); }
        else if (op == 6) { carregar_tudo(); printf("Dados carregados.\n"); }
    } while (op != 0);
}

/* ---- Main ---- */
int main(void) {
    /* ao iniciar, carregue dados se existirem */
    carregar_tudo();
    printf("Inicializando sistema... registros atuais: usuarios=%d livros=%d emprestimos=%d\n", countUsuarios, countLivros, countEmprestimos);
    menu_principal();
    /* salvar automatico ao sair */
    salvar_tudo();
    printf("Dados salvos. Encerrando.\n");
    return 0;
}