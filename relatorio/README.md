# Gerar o relatório

Coloque as bases reais em `dados/` (veja `dados/README.md`). Para repetir a coleta usada neste relatório e regenerar tabelas e gráficos:

```sh
python3 experimentos/executar.py --sizes 1000,10000 --repetitions 10
python3 experimentos/analise.py
```

O segundo comando calcula média, desvio padrão amostral, diferenças percentuais e testes t pareados. Os gráficos são escritos em `experimentos/graficos/`; na ausência de matplotlib, o script cria SVGs com a biblioteca padrão e os converte para PDF quando `rsvg-convert` está disponível. Com matplotlib instalado, são geradas versões PNG e PDF. Para instalar a dependência opcional, use um ambiente virtual e `pip install -r experimentos/requirements.txt`.

Instale `pdflatex` para compilar o documento. A partir deste diretório:

```sh
cd relatorio
pdflatex relatorio.tex
pdflatex relatorio.tex
```

As tabelas geradas ficam em `tabela_construcao.tex` e `tabela_busca.tex`; o teste t pareado é lido de `../experimentos/dados/testes_pareados.tex`. Se algum arquivo de resultado ou gráfico estiver ausente, o documento indica o item que precisa ser gerado, sem preencher resultados manualmente.
