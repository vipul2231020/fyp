from flask import Flask, render_template, request
import pickle
import pandas as pd

app = Flask(__name__)

# Load trained model
with open("railway_fault_AI_with_confidence.pkl", "rb") as f:
    model = pickle.load(f)

status_map = {0: "Normal", 1: "Crack_Left", 2: "Crack_Right", 3: "Break"}

@app.route('/')
def home():
    return render_template('index.html', prediction=None)

@app.route('/predict', methods=['POST'])
def predict():
    try:
        # Get input from user form
        left_sensor = int(request.form['left_sensor'])
        right_sensor = int(request.form['right_sensor'])

        # Prepare input
        test_input = pd.DataFrame([[left_sensor, right_sensor]], columns=['left_sensor', 'right_sensor'])

        # Predict
        pred = model.predict(test_input)[0]
        proba = model.predict_proba(test_input)[0]
        pred_class = status_map[pred]

        # Fault percentage logic
        if pred_class == "Normal":
            fault_percentage = round((1 - proba[0]) * 100, 2)
        else:
            fault_percentage = round(proba[pred] * 100, 2)

        return render_template(
            'index.html',
            prediction=pred_class,
            fault_percentage=fault_percentage,
            left_sensor=left_sensor,
            right_sensor=right_sensor
        )

    except Exception as e:
        return f"Error: {str(e)}"

if __name__ == '__main__':
    app.run(debug=True)
