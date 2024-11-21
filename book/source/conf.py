# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'Linux Kernel(基于X86_64)'
copyright = '2024, 丁敬'
author = '丁敬'

language = 'zh_CN'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'myst_parser'
]

templates_path = ['_templates']
exclude_patterns = []

latex_elements = {
    'preamble': r'''
        \renewcommand{\contentsname}{目录}
    '''
}

source_suffix = {
    '.rst':     'restructuredtext',
    '.txt':     'restructuredtext',
    '.md' :     'markdown',
}

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'alabaster'
html_static_path = ['_static']

