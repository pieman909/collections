import kivy
from kivy.app import App
from kivy.uix.label import Label
from kivy.uix.scrollview import ScrollView
from kivy.uix.gridlayout import GridLayout
from kivy.uix.button import Button
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.slider import Slider


class Grid(GridLayout):
    def __init__(self, **kwargs):
        super(Grid, self).__init__(**kwargs)
        self.cols=3
        Box = BoxLayout(orientation='vertical',size_hint_x=None,width=70,padding=10,spacing=10)
        yo = Button(width=50,text="yo",size_hint_x=None)
        hello = Button(width=50,text="hello",size_hint_x=None)
        wassup = Button(width=50, text="wassup", size_hint_x=None)
        Box.add_widget(hello)
        Box.add_widget(yo)
        Box.add_widget(wassup)
        self.add_widget(Box)
        horBox = BoxLayout(orientation='horizontal',size_hint_y=None,height=70,spacing=10,padding=10)
        horBox.add_widget(Button(text="HELLO",size_hint_y=None,height=50))
        horBox.add_widget(Button(text="WASSUP",size_hint_y=None,height=50))
        horBox.add_widget(Button(text="itsworking",size_hint_y=None,height=50))
        rightsidegrid = GridLayout()
        rightsidegrid.cols=1
        rightsidegrid.add_widget(horBox)
        scroll = Label(text="scrollingarea",size_hint_y=0.8)
        realscroll = ScrollView(size_hint_y=0.8,size_hint_x=0.9,scroll_type=['bars'],bar_width='40dp')
        rightsidegrid.add_widget(realscroll)
        scrollingrid= GridLayout(cols=1,size_hint_y=None,spacing=10)
        scrollingrid.bind(minimum_height=scrollingrid.setter('height'))

        realscroll.add_widget(scrollingrid)


        i = 1
        while i !=25:
            vari="This is thing #"+str(i)
            scrollingrid.add_widget(Label(text=vari,size_hint_y=None,height=40))
            i= i+1
        self.add_widget(rightsidegrid)
      #  self.add_widget(hello)
#        self.add_widget(Box)





class KivyTesting(App):
    def build(self):
        return Grid()
if __name__ == '__main__':
    KivyTesting().run()
