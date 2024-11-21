# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS 		?=
SPHINXBUILD 	?= sphinx-build
SOURCEDIR 		= source
BUILDDIR 		= build
TRANSDIR 		= locales
PDFNAME 		= Learn-Linux.pdf

all:latexpdf

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help all latex latexpdf clean translate

translate:
	@sphinx-build -b gettext "$(SOURCEDIR)" "$(TRANSDIR)"
	@sphinx-intl update -p "$(TRANSDIR)" -l zh_CN
	@sphinx-intl build

# 生成 latex
latex:
	@$(SPHINXBUILD) -b latex "$(SOURCEDIR)" "$(BUILDDIR)/latex"

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
latexpdf:latex
	@cd "$(BUILDDIR)/latex" && xelatex "linuxkernelx86_64.tex"
	@cd "$(BUILDDIR)/latex" && xelatex "linuxkernelx86_64.tex"
	@mv "$(BUILDDIR)/latex/linuxkernelx86_64.pdf" "$(PDFNAME)"

clean:
	@rm -rf "$(BUILDDIR)"
