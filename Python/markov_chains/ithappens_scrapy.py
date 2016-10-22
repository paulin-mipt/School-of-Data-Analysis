#import markov_chains
from lxml import html
import bs4

from scrapy.spiders import Spider
from scrapy.crawler import CrawlerProcess

def get_info():
    class WikiSpider(Spider):
        name = "ithappens_spider"
        allowed_domains = ["http://ithappens.me"]
        start_urls = ["http://ithappens.me"]

        def parse(self, response):
            root = html.fromstring(response.body)
            for story_count, story_div in enumerate(root.xpath(".//div[@class='text']")):
                with open('ithappens/ithappens_{}.txt'.format(story_count), 'w') as fout:
                    print(html.tostring(story_div, method="text", encoding='unicode'), file=fout)

    process = CrawlerProcess({})

    process.crawl(WikiSpider)
    process.start()


