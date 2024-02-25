import os
import json
import requests
import tweepy
import datetime


def lambda_handler(event, context):
    timeStr = get_str_time(int(event["time"]))
    dt_now = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=9)))
    client = tweepy.Client(
        consumer_key=get_parameter("twitter_consumer_key"),
        consumer_secret=get_parameter("twitter_consumer_secret"),
        access_token=get_parameter("twitter_access_token"),
        access_token_secret=get_parameter("twitter_access_token_secret"),
    )
    tweetMessage = timeStr + "勉強しました！(" + dt_now.strftime("%Y/%m/%d %H:%M") + ")"
    print(tweetMessage)
    client.create_tweet(text=tweetMessage)
    return {"statusCode": 200, "body": json.dumps("")}


def get_parameter(parameterName):
    end_point = "http://localhost:2773"
    path = "/systemsmanager/parameters/get/?withDecryption=true&name=" + parameterName
    url = end_point + path
    headers = {"X-Aws-Parameters-Secrets-Token": os.environ["AWS_SESSION_TOKEN"]}
    res = requests.get(url, headers=headers)
    data = res.json()
    return data["Parameter"]["Value"]


def get_str_time(t):
    h = t // 3600
    s = t % 3600
    m = s // 60
    s = s % 60
    return (
        get_complement_str_time(h)
        + ":"
        + get_complement_str_time(m)
        + ":"
        + get_complement_str_time(s)
    )


def get_complement_str_time(t):
    if t < 10:
        return "0" + str(t)
    else:
        return str(t)
