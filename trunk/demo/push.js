
var isConnected = false;
   
function getRequest()
{
    var sendRequest;
    if(window.XMLHttpRequest) {
        sendRequest = new XMLHttpRequest();
        if(sendRequest.overrideMimeType) {
            sendRequest.overrideMimeType('text/html');
        }
    }
    else if(window.ActiveXObject) {
        try {
            sendRequest = new ActiveXObject("Microsoft.XMLHTTP");
        }
        catch(e) {
        try {
            sendRequest = new ActiveXObject("Msxml2.XMLHTTP");
          }
          catch (e) {
            alert("Could not create httpRequest");
          }
        }
    }
    return sendRequest;            
}

function send(message)
{
    if(!isConnected) { 
        openChannel();
    }
 
    if (message.length > 0) {
        var sendRequest = getRequest();
        var url = "msgbus/chatdemo";
        sendRequest.onreadystatechange = function()
        {
            document.getElementById("sStatus").innerHTML = "Sent";
            if (sendRequest.readyState == 4) {
		    /* success */
            }    
        };
        sendRequest.open('POST', url, true);
        sendRequest.send(message);
    }
}

function openChannel()
{
    var channel = getRequest();
    var url = "/msgbus/chatdemo";
    channel.multipart = true;
    channel.onload = function(e) 
    {
        var reply = document.getElementById('reply');
	if (reply.childNodes.length > 2 * 10) {
            reply.removeChild(reply.firstChild);
            reply.removeChild(reply.firstChild); /* whack br also */
        }
	line = document.createTextNode(this.responseText);
	br = document.createElement("br");
	reply.appendChild(line);
	reply.appendChild(br);
	reply.innerHTML = reply.innerHTML;
    };
    channel.onerror = function ()
    {
        isConnected = false;
        document.getElementById("cStatus").innerHTML = "Disconnected";
    }
   
    channel.open('GET', url, true);
    channel.send(null);
    isConnected = true; 
    document.getElementById("cStatus").innerHTML = "Connected";
}
  
function shouldI(e)
{
    var code;
    if (!e) var e = window.event;
    if (e.keyCode) code = e.keyCode;
    else if (e.which) code = e.which;
    var character = String.fromCharCode(code);
    if(code == 13) {
        document.getElementById("sStatus").innerHTML = "Sending";
        return true;
    }    
    else {
        document.getElementById("sStatus").innerHTML = "Typing";
        return false;
    }
}

function gogo(control, e) 
{
    if(shouldI(e)) {
        send(control.value);
        control.value = control.defaultValue;
        return false;
    }
}
