import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier
import pickle
import numpy as np

df = pd.read_csv("railway_fault_dataset.csv")


mapping = {'Normal': 0, 'Crack_Left': 1, 'Crack_Right': 2, 'Break': 3}
df['status'] = df['status'].map(mapping)


X = df[['left_sensor', 'right_sensor']]
y = df['status']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)


model = DecisionTreeClassifier(random_state=42)
model.fit(X_train, y_train)


with open("railway_fault_AI_with_confidence.pkl", "wb") as f:
    pickle.dump(model, f)



test_input = pd.DataFrame([[1, 0]], columns=['left_sensor', 'right_sensor'])  
pred = model.predict(test_input)[0]
proba = model.predict_proba(test_input)[0]  

status_map = {0: "Normal", 1: "Crack_Left", 2: "Crack_Right", 3: "Break"}
pred_class = status_map[pred]


if pred_class == "Normal":
    fault_percentage = round((1 - proba[0]) * 100, 2)
else:
    fault_percentage = round(proba[pred] * 100, 2)


print(f"Prediction: {pred_class}")
print(f"Fault Percentage: {fault_percentage}%")
