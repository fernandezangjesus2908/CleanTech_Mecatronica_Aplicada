#import webbrowser
Modelo_auto = [0, "Toyota Corolla", "Honda Civic", "Ford Ranger", "Chevrolet Aveo", "Nissan Versa"]
Placa_auto  = [0, "ABC123", "DEF456", "MNO345", "PQR678", "STU901"]
Marca_auto  = [0, "Toyota", "Honda", "Ford", "Chevrolet", "Nissan"]
Agno_auto   = [0, 2020, 2019, 2018, 2021, 2022]

Modelo_moto = [0, "Yamaha FZ", "Bajaj Pulsar", "Honda CB", "Suzuki Gixxer", "Italika FT"]
Placa_moto  = [0, "GHI789", "JKL012", "VWX234", "YZA567", "BCD890"]
Marca_moto  = [0, "Yamaha", "Bajaj", "Honda", "Suzuki", "Italika"]
Agno_moto   = [0, 2022, 2021, 2020, 2019, 2018]

Datos_Auto=[]
Datos_Moto=[]
Sub_Menu=["Encender el aire acondicionado del auto","Tocar la bocina de la moto"]
Menu= [0,"Agregar Vehiculo" , "Eliminar Vehiculo", "Mostrar todos los Vehiculo","Funciones especiales","Salir"]
S_Tipo= "¿Que tipo de vehiculo elegiras? (Moto/Auto): "
class Vehiculo:
    def __init__(self, Modelo_Del_Vehiculo, Placa_Del_Vehiculo, Marca_Del_Vehiculo,Agno_Del_Vehiculo):
        self.Modelo_Del_Vehiculo = Modelo_Del_Vehiculo
        self.Placa_Del_Vehiculo = Placa_Del_Vehiculo
        self.Marca_Del_Vehiculo = Marca_Del_Vehiculo
        self.Agno_Del_Vehiculo = Agno_Del_Vehiculo

class Automovil(Vehiculo):
    def __init__(self,Modelo_Del_Vehiculo,Placa_Del_Vehiculo,Marca_Del_Vehiculo,Agno_Del_Vehiculo):
        super().__init__(Modelo_Del_Vehiculo,Placa_Del_Vehiculo,Marca_Del_Vehiculo,Agno_Del_Vehiculo)
    def __str__(self):
        Mensaje = f"Usted ha seleccionado el modelo: {self.Modelo_Del_Vehiculo}\nCon las siguientes Placas: {self.Placa_Del_Vehiculo}"
        Mensaje_2 = f"De la Marca{self.Marca_Del_Vehiculo}\nDel año {self.Agno_Del_Vehiculo}"
        return f"{Mensaje}\n{Mensaje_2}"
    def Encender_Aire(self):
         print(f"Encendiendo el Aire Acondicionado del {self.Modelo_Del_Vehiculo}...")
         print(f"¡Usted ha encendido el aire acondicionado!")

class Motocicleta(Vehiculo):
    def __init__(self,Modelo_Del_Vehiculo,Placa_Del_Vehiculo,Marca_Del_Vehiculo,Agno_Del_Vehiculo):
        super().__init__(Modelo_Del_Vehiculo,Placa_Del_Vehiculo,Marca_Del_Vehiculo,Agno_Del_Vehiculo)
    def __str__(self):
        Mensaje = f"Usted ha seleccionado el modelo: {self.Modelo_Del_Vehiculo}\nCon las siguientes Placas: {self.Placa_Del_Vehiculo}"
        Mensaje_2 = f"De la Marca: {self.Marca_Del_Vehiculo}\nDel año {self.Agno_Del_Vehiculo}"
        return f"{Mensaje}\n{Mensaje_2}"    
    def Encender_Bocina(self):
        print(f"Tocando la bocina del {self.Modelo_Del_Vehiculo}...")
        # url = "https://www.youtube.com/shorts/TEX1CfVN60w"
        # webbrowser.open(url)

def Preguntar_al_usuario(Menu_int):
    print(f"Escogiste la opcion {Menu[Menu_int]}")
    
def elegir_Vehiculo(Tipo_de_Vehiculo,Modelo,Placas,Agno,Marca):
    print("¿Que vehiculo deseas Agregar?")
    Modelo_P=input("¿Cual es el modelo del vehiculo?: ")
    Modelo.append(Modelo_P)
    Placas_P=input(f"¿Cuales son las placas de ({Modelo_P})?: ")
    Placas.append(Placas_P)
    Agno_P=int(input(f"¿De que año es ({Modelo_P})?: "))
    Agno.append(Agno_P)
    Marca_P=input(f"¿Cual es la marca de ({Modelo_P})?: ")
    Marca.append(Marca_P)

    if Tipo_de_Vehiculo == "moto":
        Datos_Moto.append(Motocicleta(Modelo_P, Placas_P, Marca_P, Agno_P))
    else:
        Datos_Auto.append(Automovil(Modelo_P, Placas_P, Marca_P, Agno_P))

    print(f"Los datos del modelo {Modelo_P} son:\nDel año {Agno_P}, de la {Marca_P} con las siguientes placas: {Placas_P}.")

def Eliminar_Vehiculo(nombre,Lista_Objetos,Modelo,Placas,Agno,Marca):
    eliminado = False
    for idx, vehiculo in enumerate(Lista_Objetos):
        if vehiculo.Modelo_Del_Vehiculo == nombre:
            Lista_Objetos.pop(idx)
            for j, m in enumerate(Modelo):
                if m == nombre:
                    Modelo.pop(j)
                    Placas.pop(j)
                    Agno.pop(j)
                    Marca.pop(j)
                    break
            eliminado = True
            print(f"El Vehiculo '{nombre}' fue eliminado correctamente.")
            break
    if not eliminado:
        print(f"No se encontró ningún Vehiculo llamado '{nombre}'.")
        
for i in range(1,len(Modelo_auto)):
    Datos_Auto.append(Automovil(Modelo_auto[i],Placa_auto[i], Marca_auto[i], Agno_auto[i]))
for i in range(1,len(Modelo_moto)):
    Datos_Moto.append(Motocicleta(Modelo_moto[i],Placa_moto[i], Marca_moto[i], Agno_moto[i])) 
print("Bienvenido al sistema de administración de vehiculos...")
while True:
    print("Tu menu de opciones es el siguiente: ")
    for i in range(1,len(Menu)):
        print(f"{i}).- {Menu[i]}")

    Seleccion = int(input("Por favor seleccione la opcion mediante indicación numerica: "))
    if Seleccion==1:
        Preguntar_al_usuario(Seleccion)
        Tipo_de_Vehiculo=input(S_Tipo)
        if Tipo_de_Vehiculo.casefold()=="moto":
            elegir_Vehiculo("moto",Modelo_moto,Placa_moto,Agno_moto,Marca_moto)
        elif Tipo_de_Vehiculo.casefold()=="auto":
            elegir_Vehiculo("auto",Modelo_auto,Placa_auto,Agno_auto,Marca_auto)
    elif Seleccion==2:
        Preguntar_al_usuario(Seleccion)
        Tipo_de_Vehiculo=input(S_Tipo)
        nombre = input("Ingrese el modelo  que desea eliminar: ").strip()
        if Tipo_de_Vehiculo.casefold()=="moto":
            Eliminar_Vehiculo(nombre,Datos_Moto,Modelo_moto,Placa_moto,Agno_moto,Marca_moto)
        elif Tipo_de_Vehiculo.casefold() == "auto":
            Eliminar_Vehiculo(nombre,Datos_Auto,Modelo_auto,Placa_auto,Agno_auto,Marca_auto)             
    elif Seleccion==3:
        Preguntar_al_usuario(Seleccion)
        print(f"Selecciona los nombres que quieres conocer")
        for Instancia in Datos_Auto:
            print(Instancia)
            print("-" * 80)
        for Instancia in Datos_Moto:
            print(Instancia)
            print("-" * 80)
    elif Seleccion==4:
        Preguntar_al_usuario(Seleccion)
        print("Escoge una de las dos opciones indicando con su respectivo numero: ")
        for i in range(len(Sub_Menu)):
            print(f"{i+1}. - {Sub_Menu[i]}.")
        Sub_Seleccion=int(input("Opción: "))
        if Sub_Seleccion == 1:
            for Instancia in Datos_Auto:
                Instancia.Encender_Aire()
        elif Sub_Seleccion == 2:
            for Instancia in Datos_Moto:
                Instancia.Encender_Bocina()
    elif Seleccion==5:
        Preguntar_al_usuario(Seleccion)
        print("Usted a escogido finalizar con el menu de opciones\nHasta luego.")
        break
    elif Seleccion>5:
        print("Favor de escoger un numero del 1-5")
    else:
        print("Por favor escoge una opcion correcta.")
exit()