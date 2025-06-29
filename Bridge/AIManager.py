from sklearn.ensemble import RandomForestRegressor
import joblib
import os
from Pot import Pot

# YOLO stuff yet to implement

class AIManager:
    sensors_regress_model : RandomForestRegressor                       # model to be used for regressoin on sensors values
    regr_model_path : str                                # path of the model weights
    MAX_SENSORS_VALUES: int                     # number of values to keep stored and to use on regressor
    yolo_model_path: str
    yool_model : None
    regr_model_loaded: bool = False

    def __init__(self,
                regr_model_path:str,
                yolo_model_path:str ):
        self.regr_model_path=regr_model_path
        self.yolo_model_path=yolo_model_path
        self.load_regr_model()


    # modify for better support
    def load_regr_model(self) :
        if os.path.exists(self.regr_model_path):
            try:
                self.sensors_regress_model = joblib.load(self.regr_model_path)
                print("Regressor model loaded successfully.")
                self.regr_model_loaded=True
                return 
            except Exception as e:
                print(f"Error loading model: {e}")
        else:
            print("Model file not found.")    


    def write_regr_model(self):
        joblib.dump(self.sensors_regress_model, self.regr_model_path)


    #
    # ADD part that does interpolation in case there is not enough data
    #
    #   
    def sensors_regress(self,pot: Pot) -> int:
        if self.regr_model_loaded:
            data=pot.data
            reg_data=[data.temperature,data.humidity,data.light,data.moisture,data.aqi]
            prediction=self.sensors_regress_model.predict(reg_data)
            return int(prediction[0,0])
        return 0



    def is_regr_loaded(self)-> bool:
        return self.regr_model_loaded