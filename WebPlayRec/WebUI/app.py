from flask import Flask, render_template, request

app = Flask(__name__)


@app.route('/')
def home():
    return render_template('index.html')


@app.route('/buttonPressed', methods=['POST'])
def button_pressed():
    data = request.get_json()
    print(data)
    return {'success': True}


if __name__ == '__main__':
    app.run()
