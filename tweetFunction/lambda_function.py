import json
        
def lambda_handler(event, context):
    print(event)
    print(context)
    # tweet('Hello, world!'+event['time'])
    return {
        'statusCode': 200,
        'body': json.dumps('Hello from Lambda!')
    }
    

