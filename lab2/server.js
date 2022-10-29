//引入http模块
var http=require('http');
var fs=require('fs');
var url=require('url');
//创建服务
var server=http.createServer(function(req,res){
    var url=req.url;
    if(url==='/'){
        fs.readFile('lab2.html',(err,data)=>{
            if(err){
                res.setHeader('Content-Type','text/plain;charset=utf-8');
				res.end('文件读取失败');
            }else{
				res.setHeader('Content-Type','text/html;charset=utf-8');
				res.end(data);
            }
        });
    }else {
		fs.readFile('SHOXIE.jpg',(err,data)=>{
			if(err){
				res.setHeader('Content-Type','text/plain;charset=utf-8');
				res.end('文件读取失败');
			}else{
				res.setHeader('Content-Type','image/jpeg');
				res.end(data);
			}
		});
    }
}).listen(8001);
console.log('Server running at http://127.0.0.1:8001/');