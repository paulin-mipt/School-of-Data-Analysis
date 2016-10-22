# import markov_chains
from lxml import html
from urllib import request
import bs4
import os


def get_info(page_id, html_content):
    root = html.fromstring(html_content)
    for story_count, story_div in enumerate(root.xpath(".//div[@class='text']")):
        filename = 'ithappens/story_{page}_{story}.txt'.format(page=page_id, story=story_count)
        with open(filename, 'w', encoding='utf-8') as fout:
            print(html.tostring(story_div, method="text", encoding='unicode'), file=fout)


def scrape_webpages():
    for page_id in range(1, 1000):
        content = request.urlopen("http://ithappens.me/page/{}".format(page_id)).read()
        get_info(page_id, content)


def merge_pages():
    with open('input.txt', 'w', encoding='utf8') as train_data:
        print("generate --depth 2 --size 100", file=train_data)
        for d, dirs, files in os.walk('ithappens'):
            for file in files:
                for line in open('ithappens/{}'.format(file), encoding='utf8').readlines():
                    if line.strip():
                        print(line, file=train_data)

# scrape_webpages()
# merge_pages()
# markov_chains.main()
