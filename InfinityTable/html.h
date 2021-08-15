
//- This is the HTML/Javascript page sent to the web browser.
//- I've not programmed Javascript very much, so not sure how well I'm following best practices n such

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: left;
  }
  h1 {
    font-size: 1.8rem;
    color: white;
  }
  h2{
    font-size: 1.5rem;
    font-weight: bold;
    color: #143642;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  hr.dashed {
    border-top: 3px dashed #bbb;
  }
  hr.dotted {
    border-top: 3px dotted #bbb;
  }
  hr.solid {
    border-top: 3px solid #bbb;
  }
  hr.rounded {
    border-top: 8px solid #bbb;
    border-radius: 5px;
  }
  </style>
<title>Infinity Table</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
</head>
<body>
  <div class="topnav">
    <h1>Infinity Table</h1>
  </div>
  <div>
    <p>
    <label>Max Brightness: <span id="maxBrightnessDisplay"></span></label>
    <input type="range" min="1" max="255" value="255" id="maxBrightnessSlider" style="width: 70%%;">
    </p>
    <p>
    <label>Animation Speed: <span id="scrollRateDisplay"></span></label>
    <input type="range" min="1" max="100" value="50" id="scrollRateSlider" style="width: 70%%;">
    </p>
    <p>
    <label>Red: <span id="redDisplay"></span></label>
    <input type="range" min="0" max="255" value="125" id="redSlider" style="width: 70%%;">
    </p>
    <p>
    <label>Green: <span id="greenDisplay"></span></label>
    <input type="range" min="0" max="255" value="125" id="greenSlider" style="width: 70%%;">
    </p>
    <p>
    <label>Blue: <span id="blueDisplay"></span></label>
    <input type="range" min="0" max="255" value="125" id="blueSlider" style="width: 70%%;">
    </p>
    <hr class="dotted">
        <input type="radio" id="mode0" name="modeRadios" value="0">
        <label for="mode0">Rainbow</label><br>
    <hr class="dotted">
        <p>
        <input type="radio" id="mode1" name="modeRadios" value="1">
        <label for="mode1">Text</label>
        <label>Text:</label>
        <input type="text" id="textValue" value="Infinity Table">
        </p>
    <hr class="dotted">
        <input type="radio" id="mode2" name="modeRadios" value="2">
        <label for="mode2">Solid Color</label>
    <hr class="dotted">
        <input type="radio" id="mode3" name="modeRadios" value="3">
        <label for="mode3">Parallelogram</label>
    <hr class="dotted">
        <input type="radio" id="mode4" name="modeRadios" value="4">
        <label for="mode4">Flashy Rainbow</label>
    <hr class="dotted">
        <input type="radio" id="mode5" name="modeRadios" value="5">
        <label for="mode5">Flashy Color</label>
    <hr class="dotted">
        <input type="radio" id="mode6" name="modeRadios" value="6">
        <label for="mode6">Fire</label>
    <hr class="dotted">
        <label>Microphone</label>
        <p>
        <input type="radio" id="mode7" name="modeRadios" value="7">
        <label for="mode7">Equalizer Inverse Color</label>
        </p><p>
        <input type="radio" id="mode8" name="modeRadios" value="8">
        <label for="mode8">Equalizer Solid Color</label>
        </p><p>
        <input type="radio" id="mode9" name="modeRadios" value="9">
        <label for="mode9">Equalizer Rainbow</label>
        </p><p>
        <input type="radio" id="mode10" name="modeRadios" value="10">
        <label for="mode10">Equalizer Full Table</label>
        </p><p>
        <input type="radio" id="mode11" name="modeRadios" value="11">
        <label for="mode11">Ambient</label>
        </p><p>
        <button type="button" id= "noiseResetButton">Generate Noise Floor</button>
        </p>
    <hr class="dotted">
        <input type="radio" id="mode12" name="modeRadios" value="12">
        <label for="mode12">OFF</label>
    <hr class="dotted">
        <button type="button" id= "softwareUpdateButton">Update Software</button>
  </div>
<script>
    class SequencedControl {
        key;
        inputID;
        displayID;
        seq;
        sendSeq;
        constructor(key, inputID, displayID) {
            this.key        = key;
            this.inputID    = inputID;
            this.displayID  = displayID;
            this.seq        = 0;
            this.sendSeq    = 0;
        }
    }
    var gateway = "ws://%IP_ADDR_PLACEHOLDER%/ws";
    var websocket;
    let maxBrightSlider   = new SequencedControl("MB", "maxBrightnessSlider", "maxBrightnessDisplay");
    let scrollSpeedSlider = new SequencedControl("SS", "scrollRateSlider", "scrollRateDisplay");
    let redSlider         = new SequencedControl("Red", "redSlider", "redDisplay");
    let greenSlider       = new SequencedControl("Green", "greenSlider", "greenDisplay");
    let blueSlider        = new SequencedControl("Blue", "blueSlider", "blueDisplay");
    let inputText         = new SequencedControl("DT", "textValue", "");
    var SSSliderSeq = 0;
    var SSSliderSendSeq = 0;
    var TextInputSeq = 0;
    var TextInputSendSeq = 0;
    var ModeRadioSeq = 0;
    var ModeRadioSendSeq = 0;
    window.addEventListener('load', onLoad);
    function initWebSocket() 
    {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(gateway);
        websocket.onopen    = onOpen;
        websocket.onclose   = onClose;
        websocket.onmessage = onMessage; // <-- add this line
    }
    function onOpen(event) 
    {
        console.log('Connection opened');
    }
    function onClose(event) 
    {
        console.log('Connection closed');
        setTimeout(initWebSocket, 2000);
    }
    function CheckSequencedStatus(input, key, value)
    {
        if (key == input.key && input.seq == input.sendSeq)
        {
            document.getElementById(input.inputID).value = value;
            if (input.displayID != "")
            {
                document.getElementById(input.displayID).innerHTML = value;
            }
        }
    }
    function onMessage(event) 
    {
        var split_pipe = event.data.split('|');
        var i;
        //console.log(split_pipe);
        for (i = 0; i < split_pipe.length; i++) 
        {
            var split_equal = split_pipe[i].split('=');
            //console.log(split_equal);
            CheckSequencedStatus(maxBrightSlider, split_equal[0], split_equal[1]);
            CheckSequencedStatus(scrollSpeedSlider, split_equal[0], split_equal[1]);
            CheckSequencedStatus(redSlider, split_equal[0], split_equal[1]);
            CheckSequencedStatus(greenSlider, split_equal[0], split_equal[1]);
            CheckSequencedStatus(blueSlider, split_equal[0], split_equal[1]);
            //- inputText isn't sent in status right now
            if (split_equal[0] == "Mode")
            {
                if (ModeRadioSeq == ModeRadioSendSeq)
                {
                    radioMode = document.getElementById("mode" + split_equal[1]);
                    radioMode.checked = true;
                }
            }
            if (split_equal[0] == "Log")
            {
                console.log(split_equal[1]);
            }
        }
    }
    function CheckSequencedInputSeq(input)
    {
        var sequence = input.seq;
        if (sequence != input.sendSeq)
        {
            var value = document.getElementById(input.inputID).value;
            console.log(input.key + "=" + value + " - " + sequence);
            websocket.send(input.key + "=" + value);
            input.sendSeq = sequence;
        }
    }
    function CommandUpdate()
    {
        CheckSequencedInputSeq(maxBrightSlider);
        CheckSequencedInputSeq(scrollSpeedSlider);
        CheckSequencedInputSeq(redSlider);
        CheckSequencedInputSeq(greenSlider);
        CheckSequencedInputSeq(blueSlider);
        CheckSequencedInputSeq(inputText);
        //- Check mode radios
        if (ModeRadioSeq != ModeRadioSendSeq)
        {
            var radios = document.getElementsByName("modeRadios");
            for (var i = 0; i < radios.length; i++) 
            {
              if (radios[i].checked) 
              {
                websocket.send("Mode=" + radios[i].value);
                console.log("Mode: " + radios[i].value);
                break;
              }
            }
            ModeRadioSendSeq = ModeRadioSeq;
        }
        setTimeout(CommandUpdate, 100);
    }
    function SequencedInput(input)
    {
        input.seq += 1;
        if (input.displayID != "")
        {
            document.getElementById(input.displayID).innerHTML = document.getElementById(input.inputID).value;
        }
    }
    function onLoad(event)
    {
        initWebSocket();
        document.getElementById("maxBrightnessSlider").oninput  = function(){SequencedInput(maxBrightSlider);};
        document.getElementById("scrollRateSlider").oninput     = function(){SequencedInput(scrollSpeedSlider);};
        document.getElementById("redSlider").oninput            = function(){SequencedInput(redSlider);};
        document.getElementById("greenSlider").oninput          = function(){SequencedInput(greenSlider);};
        document.getElementById("blueSlider").oninput           = function(){SequencedInput(blueSlider);};
        document.getElementById("textValue").oninput            = function(){SequencedInput(inputText);};
        document.getElementById("mode0").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode1").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode2").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode3").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode4").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode5").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode6").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode7").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode8").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode9").onclick                = function(){ModeRadioSeq += 1;};
        document.getElementById("mode10").onclick               = function(){ModeRadioSeq += 1;};
        document.getElementById("mode11").onclick               = function(){ModeRadioSeq += 1;};
        document.getElementById("mode12").onclick               = function(){ModeRadioSeq += 1;};
        document.getElementById("noiseResetButton").onclick     = function(){websocket.send("NoiseButton"); console.log("NoiseButton");};
        document.getElementById("softwareUpdateButton").onclick = function(){websocket.send("SoftwareUpdate"); console.log("SoftwareUpdate");};
        CommandUpdate();
    }
</script>
</body>
</html>
)rawliteral";
